#include "variable_plugin.h"

namespace ergo::studio::plugins::variable {
namespace {
const char* const kEngine = "engine";
const char* const kUi     = "ui";
} // namespace

VariablePlugin::VariablePlugin()
    : info_{"variable", "Variable Editor",
            "Live-edit variables exposed by engine clients via BIND_VAR() (ergo_bind), grouped by ergo_actor.",
            "\xF0\x9F\x8E\x9B",
            "games and clients: ws://{addr}/variable/ws (first frame {\"op\":\"hello\",\"role\":\"engine\"|\"ui\"})"} {}

net::HttpResponse VariablePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (sub == "/api/health") return json_response(200, snapshot());
    std::lock_guard<std::mutex> lk(mtx_);
    if (sub == "/api/vars") return json_response(200, registry_.vars_json());
    if (sub == "/api/actors") return json_response(200, registry_.actors_json());
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

net::WsHandlers VariablePlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text  = [this, id](const std::string& t) { on_text(id, t); };
    h.on_close = [this, id] { on_close(id); };
    return h;
}

void VariablePlugin::on_text(uint64_t id, const std::string& text) {
    const auto msg = parse_message(text);
    if (!msg) return;
    const std::string op = op_of(*msg);

    if (op == "hello") {
        const JsonValue* role = field(*msg, "role");
        const std::string r   = role ? role->as_string() : std::string();
        if (r == kEngine) {
            const JsonValue* app = field(*msg, "app");
            clients_.set_role(id, kEngine);
            std::lock_guard<std::mutex> lk(mtx_);
            apps_[id] = app && app->is_string() ? app->s : std::string("anonymous");
        } else if (r == kUi) {
            clients_.set_role(id, kUi);
            JsonValue actors, vars;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                actors = registry_.actors_json();
                vars   = registry_.vars_json();
            }
            clients_.send(id, to_json(jobj({{"op", jstr("actors")}, {"nodes", actors}})));
            clients_.send(id, to_json(jobj({{"op", jstr("registry")}, {"vars", vars}})));
        }
        return;
    }

    const std::string role = clients_.role(id);
    if (role == kEngine) {
        on_engine(id, op, *msg);
    } else if (role == kUi && op == "set") {
        const JsonValue* name  = field(*msg, "name");
        const JsonValue* value = field(*msg, "value");
        if (name && name->is_string() && value) request_set(name->s, *value);
    }
}

void VariablePlugin::on_engine(uint64_t id, const std::string& op, const JsonValue& msg) {
    bool registry_changed = false, actors_changed = false;
    std::string value_frame;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const std::string app = apps_[id];
        if (op == "bind") {
            registry_changed = registry_.bind(id, app, msg);
        } else if (op == "value") {
            const JsonValue* name  = field(msg, "name");
            const JsonValue* value = field(msg, "value");
            if (name && value && registry_.set_value(name->as_string(), *value)) {
                value_frame = to_json(jobj({{"op", jstr("value")}, {"name", *name}, {"value", *value}}));
            }
        } else if (op == "unbind") {
            const JsonValue* name = field(msg, "name");
            registry_changed      = name && registry_.unbind(name->as_string());
        } else if (op == "actor_register") {
            actors_changed = registry_.register_actor(id, app, msg);
        } else if (op == "actor_unregister") {
            const JsonValue* h = field(msg, "handle");
            actors_changed     = h && registry_.unregister_actor(static_cast<int64_t>(h->as_number()));
        }
    }
    if (registry_changed || actors_changed || !value_frame.empty()) ++revision_;
    if (!value_frame.empty()) clients_.broadcast(value_frame, kUi);
    if (actors_changed) broadcast_actors();
    if (registry_changed) broadcast_registry();
}

void VariablePlugin::on_close(uint64_t id) {
    const bool was_engine = clients_.role(id) == kEngine;
    clients_.remove(id);
    if (!was_engine) return;
    VariableRegistry::Purged p;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        p = registry_.purge_owner(id);
        apps_.erase(id);
    }
    if (p.vars || p.actors) ++revision_;
    if (p.actors) broadcast_actors();
    if (p.vars) broadcast_registry();
}

void VariablePlugin::broadcast_registry() {
    JsonValue vars;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        vars = registry_.vars_json();
    }
    clients_.broadcast(to_json(jobj({{"op", jstr("registry")}, {"vars", vars}})), kUi);
}

void VariablePlugin::broadcast_actors() {
    JsonValue nodes;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        nodes = registry_.actors_json();
    }
    clients_.broadcast(to_json(jobj({{"op", jstr("actors")}, {"nodes", nodes}})), kUi);
}

JsonValue VariablePlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(true)},
                 {"vars", jnum(static_cast<double>(registry_.var_count()))},
                 {"actors", jnum(static_cast<double>(registry_.actor_count()))},
                 {"clients", jnum(static_cast<double>(clients_.size()))}});
}

std::vector<BoundVar> VariablePlugin::vars() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return registry_.vars();
}

std::vector<ActorNode> VariablePlugin::actors() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return registry_.actors();
}

bool VariablePlugin::request_set(const std::string& name, const JsonValue& value) {
    std::optional<uint64_t> owner;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        owner = registry_.owner_of(name);
    }
    if (!owner) return false;
    clients_.send(*owner, to_json(jobj({{"op", jstr("set")}, {"name", jstr(name)}, {"value", value}})));
    return true;
}

std::unique_ptr<host::IPlugin> make_variable_plugin(const host::LogFn&) { return std::make_unique<VariablePlugin>(); }

} // namespace ergo::studio::plugins::variable
