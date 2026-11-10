#pragma once
/// "scene_editor" plugin: ergo_scene_link engines push whole scenes over
/// /scene_editor/ws ({op:"scene", id, json}); editors get SceneInfo
/// snapshots and send field patches, which go to the engine that owns the
/// scene. The engine applies them and re-publishes, so editors only ever
/// show engine state. Spec: spec/feature/tool/scene_editor.md.
#include "../common/client_set.h"
#include "parse_scene.h"

#include "ergo/studio/host/plugin.h"

#include <atomic>
#include <map>
#include <mutex>

namespace ergo::studio::plugins::scene_editor {

class SceneEditorPlugin final : public host::IPlugin, public host::IPluginHttp, public host::IPluginSocket,
                                public host::IPluginHealth {
public:
    explicit SceneEditorPlugin(host::LogFn log);

    const host::PluginInfo& info() const override { return info_; }
    host::IPluginHttp*      http() override { return this; }
    host::IPluginSocket*    socket() override { return this; }
    host::IPluginHealth*    health() override { return this; }

    net::HttpResponse handle(const net::HttpRequest& req, const std::string& sub_path) override;
    net::WsHandlers   on_open(const net::HttpRequest& req, std::shared_ptr<net::IWsConnection> conn) override;
    JsonValue         snapshot() const override;

    // ---- In-process editor API ----
    std::vector<SceneInfo> scenes() const;
    /// Forward a patch to the scene's owner. False when the scene is unknown.
    bool     patch(const std::string& scene_id, const std::string& actor_id, const std::string& field,
                   const JsonValue& value);
    uint64_t revision() const { return revision_.load(); }

private:
    struct Stored {
        SceneInfo info;
        uint64_t  owner = 0;
    };
    void      on_text(uint64_t id, const std::string& text);
    void      on_close(uint64_t id);
    JsonValue scene_list_json() const;   // mtx_ held by caller

    host::PluginInfo              info_;
    host::LogFn                   log_;
    ClientSet                     clients_;
    mutable std::mutex            mtx_;
    std::map<std::string, Stored> scenes_;
    std::map<uint64_t, std::string> apps_;
    std::atomic<uint64_t>         revision_{0};
};

std::unique_ptr<host::IPlugin> make_scene_editor_plugin(const host::LogFn& log);

} // namespace ergo::studio::plugins::scene_editor
