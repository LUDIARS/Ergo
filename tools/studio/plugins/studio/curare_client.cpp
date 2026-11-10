#include "curare_client.h"

#include "concordia_client.h"

#include "ergo/studio/platform/http_client.h"

namespace ergo::studio::plugins::studio {
namespace {
using platform::url_encode;

ServiceResult listed(ServiceResult r, const char* key) {
    if (r.ok) r.data = list_of(r.data, key);
    return r;
}

JsonValue strings(const std::vector<std::string>& v) {
    JsonValue a = JsonValue::make_array();
    for (const auto& s : v) a.push(jstr(s));
    return a;
}
} // namespace

std::map<std::string, std::string> CurareClient::headers() const {
    if (!token_) return {};
    return {{"Authorization", "Bearer " + *token_}};
}

ServiceResult CurareClient::get(const std::string& path) const {
    return caller_.call(ep_.url(Service::Curare), path, "GET", JsonValue::make_null(), headers());
}

ServiceResult CurareClient::me() const { return get("/api/v1/me"); }
ServiceResult CurareClient::projects() const { return listed(get("/api/v1/projects"), "projects"); }

ServiceResult CurareClient::folders(const std::string& project_id) const {
    return listed(get("/api/v1/projects/" + url_encode(project_id) + "/folders"), "folders");
}

ServiceResult CurareClient::assets(const std::string& project_id, const std::string& folder_id) const {
    return listed(get("/api/v1/projects/" + url_encode(project_id) + "/folders/" + url_encode(folder_id) + "/assets"),
                  "assets");
}

ServiceResult CurareClient::versions(const std::string& asset_id) const {
    return listed(get("/api/v1/assets/" + url_encode(asset_id) + "/versions"), "versions");
}

ServiceResult CurareClient::plan(const std::vector<std::string>& version_ids, const std::vector<std::string>& known_hashes) const {
    return caller_.call(ep_.url(Service::Curare), "/api/v1/acquisition/plan", "POST",
                        jobj({{"versionIds", strings(version_ids)}, {"knownHashes", strings(known_hashes)}}), headers());
}

} // namespace ergo::studio::plugins::studio
