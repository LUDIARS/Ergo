#include "scene_editor_plugin.h"

namespace ergo::studio::plugins::scene_editor {
namespace {
const char* const kEngine = "engine";
const char* const kUi     = "ui";

std::string str_field(const JsonValue& msg, const char* key, bool* ok = nullptr) {
    const JsonValue* v = field(msg, key);
    if (ok) *ok = v && v->is_string();
    return v && v->is_string() ? v->s : std::string();
}
} // namespace

SceneEditorPlugin::SceneEditorPlugin(host::LogFn log)
    : info_{"scene_editor", "Scene Editor",
            "Scene tree \xC2\xB7 actor inspector \xC2\xB7 live WS patch relay for ergo_scene clients",
            "\xF0\x9F\x8E\xAC"},
      log_(std::move(log)) {}

net::HttpResponse SceneEditorPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/scenes") {
        std::lock_guard<std::mutex> lk(mtx_);
        return json_response(200, scene_list_json());
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

net::WsHandlers SceneEditorPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text  = [this, id](const std::string& t) { on_text(id, t); };
    h.on_close = [this, id] { on_close(id); };
    return h;
}

void SceneEditorPlugin::on_text(uint64_t id, const std::string& text) {
    const auto msg = parse_message(text);
    if (!msg) return;
    const std::string op = op_of(*msg);

    if (op == "hello") {
        const std::string role = str_field(*msg, "role");
        if (role == kEngine) {
            bool        has_app = false;
            std::string app     = str_field(*msg, "app", &has_app);
            if (!has_app) app = "anonymous";
            clients_.set_role(id, kEngine);
            {
                std::lock_guard<std::mutex> lk(mtx_);
                apps_[id] = app;
            }
            if (log_) log_("info", "[scene_editor] engine connected: " + app);
        } else if (role == kUi) {
            clients_.set_role(id, kUi);
            JsonValue list;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                list = scene_list_json();
            }
            clients_.send(id, to_json(jobj({{"op", jstr("scenes")}, {"scenes", list}})));
        }
        return;
    }

    const std::string role = clients_.role(id);
    if (role == kEngine) {
        bool id_ok = false;
        const std::string scene_id = str_field(*msg, "id", &id_ok);
        if (!id_ok) return;
        std::string frame;
        if (op == "scene") {
            bool json_ok = false;
            const std::string json = str_field(*msg, "json", &json_ok);
            if (!json_ok) return;
            std::lock_guard<std::mutex> lk(mtx_);
            auto parsed = parse_scene(scene_id, json, apps_[id]);
            if (!parsed) return;
            frame = to_json(jobj({{"op", jstr("scene_update")}, {"scene", to_json_value(*parsed)}}));
            scenes_[scene_id] = {std::move(*parsed), id};
        } else if (op == "scene_remove") {
            std::lock_guard<std::mutex> lk(mtx_);
            if (scenes_.erase(scene_id) == 0) return;
            frame = to_json(jobj({{"op", jstr("scene_removed")}, {"id", jstr(scene_id)}}));
        } else {
            return;
        }
        ++revision_;
        clients_.broadcast(frame, kUi);
    } else if (role == kUi && op == "patch") {
        const JsonValue* value = field(*msg, "value");
        patch(str_field(*msg, "scene_id"), str_field(*msg, "actor_id"), str_field(*msg, "field"),
              value ? *value : JsonValue::make_null());
    }
}

void SceneEditorPlugin::on_close(uint64_t id) {
    const bool was_engine = clients_.role(id) == kEngine;
    clients_.remove(id);
    if (!was_engine) return;
    std::vector<std::string> removed;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        apps_.erase(id);
        for (auto it = scenes_.begin(); it != scenes_.end();) {
            if (it->second.owner == id) { removed.push_back(it->first); it = scenes_.erase(it); }
            else ++it;
        }
    }
    if (!removed.empty()) ++revision_;
    for (const auto& sid : removed) {
        clients_.broadcast(to_json(jobj({{"op", jstr("scene_removed")}, {"id", jstr(sid)}})), kUi);
    }
}

bool SceneEditorPlugin::patch(const std::string& scene_id, const std::string& actor_id, const std::string& f,
                              const JsonValue& value) {
    uint64_t owner = 0;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = scenes_.find(scene_id);
        if (it == scenes_.end()) return false;
        owner = it->second.owner;
    }
    clients_.send(owner, to_json(jobj({{"op", jstr("patch")}, {"scene_id", jstr(scene_id)},
                                       {"actor_id", jstr(actor_id)}, {"field", jstr(f)}, {"value", value}})));
    return true;
}

JsonValue SceneEditorPlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(true)}, {"scenes", jnum(static_cast<double>(scenes_.size()))},
                 {"clients", jnum(static_cast<double>(clients_.size()))}});
}

std::vector<SceneInfo> SceneEditorPlugin::scenes() const {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<SceneInfo> out;
    for (const auto& [k, s] : scenes_) out.push_back(s.info);
    return out;
}

JsonValue SceneEditorPlugin::scene_list_json() const {
    JsonValue arr = JsonValue::make_array();
    for (const auto& [k, s] : scenes_) arr.push(to_json_value(s.info));
    return arr;
}

std::unique_ptr<host::IPlugin> make_scene_editor_plugin(const host::LogFn& log) {
    return std::make_unique<SceneEditorPlugin>(log);
}

} // namespace ergo::studio::plugins::scene_editor
