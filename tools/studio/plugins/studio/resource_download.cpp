#include "resource_download.h"

#include "../common/file_io.h"
#include "resource_sync.h"

#include "ergo/studio/platform/sha256.h"

#include <system_error>

namespace ergo::studio::plugins::studio {
namespace fs = std::filesystem;
namespace {

std::string str_of(const JsonValue& v, const char* k) {
    const JsonValue* x = field(v, k);
    return x && x->is_string() ? x->s : std::string();
}

std::optional<std::string> file_hash(const fs::path& p) {
    const auto h = platform::sha256_file_hex(p);
    if (!h) return std::nullopt;
    return to_content_hash(*h);
}

} // namespace

JsonValue to_json_value(const SyncItemResult& r) {
    JsonValue o = jobj({{"name", jstr(r.name)}, {"versionId", jstr(r.version_id)}, {"result", jstr(r.result)}});
    if (!r.detail.empty()) o.set("detail", jstr(r.detail));
    return o;
}

std::vector<std::string> local_hashes(const fs::path& dir, std::size_t limit) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& ent : fs::directory_iterator(dir, ec)) {
        std::error_code e2;
        if (!ent.is_regular_file(e2) || ent.path().extension() == ".part") continue;
        if (auto h = file_hash(ent.path())) out.push_back(*h);
        if (out.size() >= limit) break;
    }
    return out;
}

std::vector<SyncItemResult> execute_plan(const JsonValue& plan, const fs::path& dir, platform::IHttpClient& http,
                                         int timeout_ms) {
    std::vector<SyncItemResult> results;
    std::error_code ec;
    fs::create_directories(dir, ec);
    const JsonValue* items = field(plan, "items");
    if (!items || !items->is_array()) return results;
    for (const auto& item : *items->a) {
        const std::string raw_name = str_of(item, "name");
        const std::string vid      = str_of(item, "versionId");
        const auto name = safe_file_name(raw_name);
        if (!name) {
            results.push_back({raw_name, vid, "invalid_name", {}});
            continue;
        }
        const fs::path path = dir / fs::u8path(*name);
        std::error_code e2;
        const auto local = fs::is_regular_file(path, e2) ? file_hash(path) : std::nullopt;
        const std::string url   = str_of(item, "downloadUrl");
        const JsonValue*  needs = field(item, "needsDownload");
        const SyncAction  act   = decide_sync_action(str_of(item, "contentHash"), needs && needs->as_bool(), !url.empty(), local);
        if (act != SyncAction::Download) {
            results.push_back({*name, vid, sync_action_name(act), {}});
            continue;
        }
        const fs::path tmp = fs::path(path).concat("." + vid.substr(0, 8) + ".part");
        platform::HttpClientRequest req;
        req.url        = url;
        req.timeout_ms = timeout_ms;
        const auto res = http.send(req);
        std::string detail;
        std::string err;
        if (!res.ok()) {
            detail = res.transport_ok ? "HTTP " + std::to_string(res.status) : res.error;
        } else if (!write_text_file(tmp, res.body, err)) {
            detail = err;
        } else if (file_hash(tmp) != str_of(item, "contentHash")) {
            detail = "内容ハッシュが計画と一致しません";
        } else {
            fs::rename(tmp, path, e2);
            if (!e2) {
                results.push_back({*name, vid, "downloaded", {}});
                continue;
            }
            detail = e2.message();
        }
        fs::remove(tmp, e2);   // never leave a broken download behind
        results.push_back({*name, vid, "failed", detail});
    }
    return results;
}

} // namespace ergo::studio::plugins::studio
