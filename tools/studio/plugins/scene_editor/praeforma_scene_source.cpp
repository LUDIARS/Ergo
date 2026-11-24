#include "scene_editor_plugin.h"

#include <algorithm>

namespace ergo::studio::plugins::scene_editor {
namespace {
bool valid_id(const std::string& id) {
    return !id.empty() && id.size() <= 120 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '-' || c == '_';
    });
}
net::HttpResponse failure(int status, const std::string& message) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(message)}}));
}

class PfSceneSource {
public:
    PfSceneSource(studio::StudioConfig cfg, std::shared_ptr<const IProfileSource> profiles)
        : cfg_(std::move(cfg)),
          profiles_(std::move(profiles)),
          caller_(platform::make_system_http_client(), cfg_.request_timeout_ms),
          endpoints_(cfg_, caller_) {}

    net::HttpResponse read(const std::string& route) {
        const auto set = profiles_->profiles();
        if (!set.errors.empty()) return failure(422, "Studio プロフィールを読み取れません");
        if (set.profiles.size() != 1 || set.profiles.front().praeforma_project.empty())
            return failure(409, "開いているプロフィールに praeformaProjectId を設定してください");
        const auto& profile = set.profiles.front();
        const auto& project = profile.praeforma_project;
        if (!valid_id(project)) return failure(422, "praeformaProjectId が不正です");
        const bool catalog = route == "/api/praeforma";
        std::string path = "/api/projects/" + project;
        if (catalog)
            path += "/scene-map";
        else {
            const std::string prefix = "/api/praeforma/" + project + "/specs/";
            if (route.compare(0, prefix.size(), prefix) != 0)
                return failure(404, "対象プロジェクトが異なります");
            const auto spec = route.substr(prefix.size());
            if (!valid_id(spec)) return failure(400, "仕様 ID が不正です");
            path += "/specs/" + spec;
        }
        const auto topology = endpoints_.snapshot().all;
        const auto pf = topology.find("PRAEFORMA");
        if (pf == topology.end() || pf->second.empty())
            return failure(503, "Praeforma の接続先を取得できません");
        const auto result = caller_.call(pf->second, path);
        if (!result.ok)
            return failure(studio::failure_http_status(result.kind),
                           "Praeforma: " + result.message);
        const auto* value = field(result.data, catalog ? "scenes" : "spec");
        if (!value || (catalog ? !value->is_array() : !value->is_object()))
            return failure(502, "Praeforma の応答形式が不正です");
        if (!catalog) return json_response(200, result.data);
        for (const auto& scene : *value->a) {
            const auto* id = field(scene, "id");
            const auto* specs = field(scene, "specs");
            if (!id || !valid_id(id->as_string()) || !specs || !specs->is_array())
                return failure(502, "Pf のシーンデータが不正です");
            for (const auto& row : *specs->a) {
                const auto* spec_id = field(row, "id");
                if (!spec_id || !valid_id(spec_id->as_string()))
                    return failure(502, "Pf の仕様データが不正です");
            }
        }
        auto out = result.data;
        out.set("projectId", jstr(project));
        out.set("bindings", profile.praeforma_scenes);
        return json_response(200, out);
    }

private:
    studio::StudioConfig cfg_;
    std::shared_ptr<const IProfileSource> profiles_;
    studio::ServiceCaller caller_;
    studio::EndpointResolver endpoints_;
};
} // namespace
std::unique_ptr<host::IPlugin> make_scene_editor_plugin(
    const studio::StudioConfig& cfg, std::shared_ptr<const IProfileSource> profiles,
    const host::LogFn& log) {
    auto source = std::make_shared<PfSceneSource>(cfg, std::move(profiles));
    return std::make_unique<SceneEditorPlugin>(
        log, [source](const std::string& route) { return source->read(route); });
}
} // namespace ergo::studio::plugins::scene_editor
