#include "rive_plugin.h"

namespace ergo::studio::plugins::rive {
namespace {
constexpr int kSchemaVersion = 1;
}

RivePlugin::RivePlugin()
    : info_{"rive", "Rive Player",
            "Drop a .riv file to play it. Inspect artboards, state machines, animations and timeline.", "\xE2\x96\xB6"} {}

net::HttpResponse RivePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/meta") {
        return json_response(200, jobj({{"meta", meta()}, {"clients", jnum(static_cast<double>(clients_.size()))}}));
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

net::WsHandlers RivePlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    clients_.add(conn);
    const uint64_t id = conn->id();
    // Everyone (the newcomer included) gets the current meta and the new count.
    broadcast();

    net::WsHandlers h;
    h.on_text = [this, id](const std::string& text) {
        const auto msg = parse_message(text);
        if (!msg) return;
        const std::string op = op_of(*msg);
        if (op == "publish") {
            const JsonValue* m = field(*msg, "meta");
            if (m) publish(*m);
        } else if (op == "ping") {
            clients_.send(id, "{\"op\":\"ack\"}");
        }
    };
    h.on_close = [this, id] {
        clients_.remove(id);
        broadcast();
    };
    return h;
}

JsonValue RivePlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(true)}, {"version", jnum(kSchemaVersion)},
                 {"clients", jnum(static_cast<double>(clients_.size()))}, {"has_meta", jbool(meta_.is_object())}});
}

JsonValue RivePlugin::meta() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return deep_copy(meta_);
}

bool RivePlugin::publish(const JsonValue& meta) {
    if (!meta.is_object()) return false;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        meta_ = deep_copy(meta);
    }
    ++revision_;
    broadcast();
    return true;
}

void RivePlugin::broadcast() {
    clients_.broadcast(
        to_json(jobj({{"op", jstr("meta")}, {"meta", meta()}, {"clients", jnum(static_cast<double>(clients_.size()))}})));
}

std::unique_ptr<host::IPlugin> make_rive_plugin(const host::LogFn&) { return std::make_unique<RivePlugin>(); }

} // namespace ergo::studio::plugins::rive
