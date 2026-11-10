#include "latest_blob_plugin.h"

#include "json_util.h"

namespace ergo::studio::plugins {

LatestBlobPlugin::LatestBlobPlugin(LatestBlobSpec spec, host::LogFn log)
    : spec_(std::move(spec)), log_(std::move(log)) {}

net::HttpResponse LatestBlobPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health" && req.method == "GET") return json_response(200, snapshot());
    if (spec_.http_publish && sub == "/api/publish" && req.method == "POST") {
        if (!publish(req.body)) {
            return json_response(400, jobj({{"ok", jbool(false)}, {"error", jstr(spec_.invalid_error)}}));
        }
        return json_response(200, jobj({{"ok", jbool(true)}, {"clients", jnum(static_cast<double>(clients_.size()))}}));
    }
    if (spec_.http_publish && sub == "/api/snapshot" && req.method == "GET") {
        const auto doc = current();
        if (!doc) return json_response(404, jobj({{"ok", jbool(false)}, {"error", jstr("no snapshot yet")}}));
        return net::HttpResponse::json(200, *doc);
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

net::WsHandlers LatestBlobPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    clients_.send(id, frame());

    net::WsHandlers h;
    h.on_text = [this, id](const std::string& text) {
        const auto msg = parse_message(text);
        if (!msg) return;
        const std::string op = op_of(*msg);
        if (op == "publish") {
            const JsonValue* doc = field(*msg, spec_.field);
            if (!doc || !doc->is_string() || !publish(doc->s)) {
                if (log_) log_("warn", spec_.info.id + ": WS publish rejected (invalid " + spec_.field + ")");
            }
        } else if (op == "ping") {
            clients_.send(id, "{\"op\":\"ack\"}");
        }
    };
    h.on_close = [this, id] { clients_.remove(id); };
    return h;
}

JsonValue LatestBlobPlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(true)}, {"version", jnum(spec_.schema_version)},
                 {"clients", jnum(static_cast<double>(clients_.size()))}, {spec_.has_key, jbool(current_.has_value())}});
}

std::optional<std::string> LatestBlobPlugin::current() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return current_;
}

bool LatestBlobPlugin::publish(const std::string& doc) {
    if (spec_.validate && !spec_.validate(doc)) {
        if (log_) log_("warn", spec_.info.id + ": rejected invalid " + spec_.field);
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        current_ = doc;
    }
    ++revision_;
    clients_.broadcast(frame());
    return true;
}

std::string LatestBlobPlugin::frame() const {
    const auto doc = current();
    return to_json(jobj({{"op", jstr(spec_.frame_op)},
                         {spec_.field, doc ? jstr(*doc) : JsonValue::make_null()},
                         {"clients", jnum(static_cast<double>(clients_.size()))}}));
}

} // namespace ergo::studio::plugins
