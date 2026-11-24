#include "ergo/studio/registry/trust.h"

#include "canonical.h"
#include "signature.h"
#include "ergo/studio/plugin_abi.h"

#include <algorithm>
#include <cmath>
#include <regex>

namespace ergo::studio::registry {
namespace {

using portus::Json;

bool hex(const std::string& value, std::size_t length) {
    return value.size() == length && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}

std::string display_text(const std::string& value) {
    std::string out;
    for (std::size_t i = 0; i < value.size();) {
        // Only parse_document can introduce the private encodings of unpaired UTF-16 code units.
        if (value.size() - i >= 3 && static_cast<unsigned char>(value[i]) == 0xed
            && static_cast<unsigned char>(value[i + 1]) >= 0xa0) {
            out += "\xef\xbf\xbd";
            i += 3;
        } else {
            out += value[i++];
        }
    }
    return out;
}

bool valid_id(const std::string& id) {
    static const std::regex shape("[a-z0-9][a-z0-9-]*/[a-z0-9][a-z0-9._-]*");
    return id.size() <= 256 && std::regex_match(id, shape);
}

bool tags(const Json& object, const char* key, std::vector<std::string>& out) {
    const auto& value = object.at(key);
    if (!value.is_array()) return value.is_null();
    for (const auto& tag : value.items()) {
        if (tag.as_string().empty()) return false;
        out.push_back(display_text(tag.as_string()));
    }
    return true;
}

bool valid_source(Entry& entry) {
    static const std::regex shape("[A-Za-z0-9][A-Za-z0-9-]*/[A-Za-z0-9._-]+:([A-Za-z0-9._-]+/)*[A-Za-z0-9._-]+@[0-9a-f]{40}");
    if (entry.source.size() > 512 || !std::regex_match(entry.source, shape)) return false;
    const auto subpath = entry.source.substr(entry.source.find(':') + 1);
    if (("/" + subpath.substr(0, subpath.find('@')) + "/").find("/../") != std::string::npos) return false;
    if (("/" + subpath.substr(0, subpath.find('@')) + "/").find("/./") != std::string::npos) return false;
    entry.rev = entry.source.substr(entry.source.size() - 40);
    return true;
}

bool valid_metadata(const Json& item) {
    static const std::regex publisher_shape("[a-z0-9][a-z0-9-]*");
    const auto& publisher = item.at("publisher").as_string();
    const auto abi = item.at("abi").as_number();
    if (item.at("title").as_string().empty() || publisher.size() > 128 || !std::regex_match(publisher, publisher_shape)
        || abi < 1 || abi > 9007199254740991.0 || std::trunc(abi) != abi) return false;
    if (const auto* summary = item.find("summary"); summary && !summary->is_string()) return false;
    if (const auto* artifacts = item.find("artifacts")) {
        if (!artifacts->is_array()) return false;
        for (const auto& asset : artifacts->items()) {
            if (asset.at("platform").as_string().empty() || asset.at("url").as_string().rfind("https://", 0) != 0
                || !hex(asset.at("sha256").as_string(), 64)) return false;
        }
    }
    return true;
}

std::vector<Entry> entries(const Json& index) {
    if (index.at("version").as_number() != 1 || !index.at("plugins").is_array())
        throw portus::JsonError("索引の形式が不正です");
    std::vector<Entry> out;
    std::set<std::string> seen;
    for (const auto& item : index.at("plugins").items()) {
        Entry e;
        e.id = item.at("id").as_string();
        e.title = display_text(item.at("title").as_string());
        e.publisher = item.at("publisher").as_string();
        e.kind = item.at("kind").as_string();
        e.source = item.at("source").as_string();
        if (!valid_id(e.id) || !seen.insert(e.id).second || !valid_metadata(item)
            || !tags(item, "domains", e.domains) || !tags(item, "styles", e.styles)
            || (e.kind == "pack" ? !valid_source(e) : e.kind != "builtin" || item.find("source")))
            throw portus::JsonError("索引の項目が不正です");
        if (item.at("abi").as_number() != ERGO_STUDIO_PLUGIN_ABI_VERSION) e.reason = "Studio の ABI と一致しません";
        out.push_back(std::move(e));
    }
    return out;
}

bool read_revocations(const Json& document, Trust& trust) {
    const auto& signers = document.at("certificates");
    const auto& versions = document.at("plugins");
    if (document.at("version").as_number() != 1 || !signers.is_array() || !versions.is_array()) return false;
    for (const auto& item : signers.items()) {
        const auto& fingerprint = item.at("fingerprint").as_string();
        if (!hex(fingerprint, 64)) return false;
        trust.revoked_signers.insert(fingerprint);
    }
    for (const auto& item : versions.items()) {
        const auto& id = item.at("id").as_string();
        const auto& rev = item.at("rev").as_string();
        if (!valid_id(id) || !hex(rev, 40)) return false;
        trust.revoked_versions.emplace(id, rev);
    }
    return true;
}

} // namespace

bool Trust::permits(const Provenance& provenance, const std::string& rev) const {
    return verified() && !provenance.id.empty() && !provenance.publisher.empty()
        && hex(provenance.fingerprint, 64) && hex(rev, 40)
        && provenance.serial > 0 && provenance.serial <= serial
        && !revoked_signers.count(provenance.fingerprint)
        && !revoked_versions.count({provenance.id, rev});
}

Trust verify_release(const Release& release, const std::string& root_pem,
                     std::uint64_t minimum_serial, std::time_t now) {
    Trust trust;
    const auto reject = [&](const char* reason) {
        trust.reason = reason;
        return std::move(trust);
    };
    try {
        const auto index = parse_document(release.index);
        trust.entries = entries(index);
        if (root_pem.empty()) return reject("信頼の起点がありません");
        const auto signer = verified_signer(root_pem, release.signing_pem, now, trust.fingerprint);
        if (!signer) return reject("署名用証明書を検証できません");
        if (!verifies(signer.get(), index)) return reject("索引の署名が一致しません");
        const auto revoked = parse_document(release.revoked);
        if (!verifies(signer.get(), revoked) || !read_revocations(revoked, trust))
            return reject("失効リストを検証できません");
        const double serial = index.at("serial").as_number();
        if (serial < 1 || serial != revoked.at("serial").as_number()
            || static_cast<std::uint64_t>(serial) < minimum_serial)
            return reject("索引と失効リストの版が一致しないか、版が巻き戻っています");
        if (trust.revoked_signers.count(trust.fingerprint)) return reject("署名用証明書は失効しています");
        for (auto& entry : trust.entries) {
            if (trust.revoked_versions.count({entry.id, entry.rev})) entry.reason = "この版は失効しています";
        }
        trust.serial = static_cast<std::uint64_t>(serial);
        return trust;
    } catch (const portus::JsonError&) {
        return reject("索引または失効リストの形式が不正です");
    }
}

} // namespace ergo::studio::registry
