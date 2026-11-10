#include "plugin_ledger.h"

#include "../common/file_io.h"
#include "../common/json_pretty.h"

#include <set>
#include <system_error>

namespace ergo::studio::plugins::studio {
namespace fs = std::filesystem;
namespace {

#ifdef _WIN32
constexpr char kSep = ';';
#else
constexpr char kSep = ':';
#endif

std::string utf8(const fs::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string norm(const std::string& d) {
    std::error_code ec;
    return utf8(fs::absolute(fs::u8path(d), ec).lexically_normal());
}

} // namespace

JsonValue to_json_value(const LedgerPack& p) {
    JsonValue plugins = JsonValue::make_array();
    for (const auto& n : p.plugins) plugins.push(jstr(n));
    return jobj({{"id", jstr(p.id)}, {"owner", jstr(p.owner)}, {"repo", jstr(p.repo)}, {"subpath", jstr(p.subpath)},
                 {"rev", jstr(p.rev)}, {"checkoutDir", jstr(p.checkout_dir)}, {"enabled", jbool(p.enabled)},
                 {"installedAt", jstr(p.installed_at)}, {"plugins", plugins}});
}

fs::path ledger_path(const fs::path& home) { return home / "plugins" / "ledger.json"; }

Ledger parse_ledger(const std::string& text) {
    Ledger l;
    JsonValue root;
    if (!ergo::common::jsonm::parse(text, root)) return l;
    const JsonValue* packs = field(root, "packs");
    if (!packs || !packs->is_array()) return l;
    for (const auto& o : *packs->a) {
        auto s = [&](const char* k) -> const JsonValue* {
            const JsonValue* v = field(o, k);
            return v && v->is_string() ? v : nullptr;
        };
        if (!s("id") || !s("owner") || !s("repo") || !s("subpath") || !s("rev") || !s("checkoutDir")) continue;
        LedgerPack p;
        p.id           = s("id")->s;
        p.owner        = s("owner")->s;
        p.repo         = s("repo")->s;
        p.subpath      = s("subpath")->s;
        p.rev          = s("rev")->s;
        p.checkout_dir = s("checkoutDir")->s;
        const JsonValue* en = field(o, "enabled");
        p.enabled      = en && en->is_bool() && en->b;
        p.installed_at = s("installedAt") ? s("installedAt")->s : std::string();
        if (const JsonValue* pl = field(o, "plugins"); pl && pl->is_array()) {
            for (const auto& n : *pl->a) {
                if (n.is_string()) p.plugins.push_back(n.s);
            }
        }
        l.packs.push_back(std::move(p));
    }
    return l;
}

Ledger read_ledger(const fs::path& home) {
    std::string err;
    const auto text = read_text_file(ledger_path(home), err);
    return text ? parse_ledger(*text) : Ledger{};
}

bool write_ledger(const fs::path& home, const Ledger& l, std::string& err) {
    OrderedJson packs = OrderedJson::array();
    for (const auto& p : l.packs) {
        OrderedJson o = OrderedJson::object();
        const JsonValue v = to_json_value(p);
        for (const char* k : {"id", "owner", "repo", "subpath", "rev", "checkoutDir", "enabled", "installedAt", "plugins"}) {
            o.add(k, OrderedJson::from(*v.find(k)));
        }
        packs.push(std::move(o));
    }
    OrderedJson root = OrderedJson::object();
    root.add("version", OrderedJson::from(jnum(kLedgerVersion))).add("packs", std::move(packs));
    const fs::path path = ledger_path(home);
    const fs::path tmp  = fs::path(path).concat(".tmp");
    if (!write_text_file(tmp, root.pretty() + "\n", err)) return false;
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        err = "ledger rename failed: " + ec.message();
        return false;
    }
    return true;
}

std::vector<std::string> enabled_pack_dirs(const Ledger& l) {
    std::vector<std::string> out;
    for (const auto& p : l.packs) {
        if (p.enabled) out.push_back(utf8(fs::u8path(p.checkout_dir) / fs::u8path(p.subpath)));
    }
    return out;
}

std::string plugin_dirs_with_ledger(const std::string& raw, const Ledger& l) {
    std::vector<std::string> dirs;
    std::set<std::string>    seen;
    std::size_t start = 0;
    while (start <= raw.size()) {
        const auto end = raw.find(kSep, start);
        std::string d = raw.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto b = d.find_first_not_of(" \t");
        if (b != std::string::npos) {
            d = d.substr(b, d.find_last_not_of(" \t") - b + 1);
            dirs.push_back(d);
            seen.insert(norm(d));
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    for (const auto& d : enabled_pack_dirs(l)) {
        if (seen.insert(norm(d)).second) dirs.push_back(d);
    }
    std::string out;
    for (const auto& d : dirs) out += (out.empty() ? "" : std::string(1, kSep)) + d;
    return out;
}

} // namespace ergo::studio::plugins::studio
