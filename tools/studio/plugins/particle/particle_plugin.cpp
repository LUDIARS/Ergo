#include "particle_plugin.h"

namespace ergo::studio::plugins::particle {

ParticlePlugin::ParticlePlugin()
    : info_{"particle", "Particle Editor",
            "Edit a ParticleEffectConfig live; broadcasts to ergo_particle clients over WS.", "\xE2\x9C\xA8"},
      current_(default_effect()) {}

net::HttpResponse ParticlePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health" && req.method == "GET") return json_response(200, snapshot());
    if (sub == "/api/effect") {
        if (req.method == "GET") return json_response(200, config());
        if (req.method == "POST") {
            JsonValue body;
            if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) {
                return net::HttpResponse::json(400, "{\"ok\":false,\"err\":\"json\"}");
            }
            set(body);
            return json_response(200, jobj({{"ok", jbool(true)}, {"config", config()}}));
        }
        return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

net::WsHandlers ParticlePlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    // The new client gets the state first, then everyone sees the new count.
    clients_.send(id, to_json(jobj({{"op", jstr("state")}, {"config", config()},
                                    {"clients", jnum(static_cast<double>(clients_.size()))}})));
    broadcast_state();

    net::WsHandlers h;
    h.on_text = [this, id](const std::string& text) {
        const auto msg = parse_message(text);
        if (!msg) return;
        const std::string op  = op_of(*msg);
        const JsonValue*  cfg = field(*msg, "config");
        if (op == "set") {
            set(cfg ? *cfg : JsonValue::make_object());
        } else if (op == "replace") {
            if (cfg) replace(*cfg);
        } else if (op == "ping") {
            clients_.send(id, "{\"op\":\"ack\"}");
        }
    };
    h.on_close = [this, id] {
        clients_.remove(id);
        broadcast_state();
    };
    return h;
}

JsonValue ParticlePlugin::snapshot() const {
    return jobj({{"ok", jbool(true)}, {"version", jnum(kSchemaVersion)},
                 {"clients", jnum(static_cast<double>(clients_.size()))}});
}

JsonValue ParticlePlugin::config() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return deep_copy(current_);
}

void ParticlePlugin::set(const JsonValue& partial) {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = merge_config(current_, partial);
    }
    ++revision_;
    broadcast_state();
}

bool ParticlePlugin::replace(const JsonValue& full) {
    if (!full.is_object()) return false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = merge_config(default_effect(), full);
    }
    ++revision_;
    broadcast_state();
    return true;
}

void ParticlePlugin::broadcast_state() {
    clients_.broadcast(to_json(jobj({{"op", jstr("state")}, {"config", config()},
                                     {"clients", jnum(static_cast<double>(clients_.size()))}})));
}

std::unique_ptr<host::IPlugin> make_particle_plugin(const host::LogFn&) { return std::make_unique<ParticlePlugin>(); }

} // namespace ergo::studio::plugins::particle
