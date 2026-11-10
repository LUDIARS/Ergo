#include "log_plugin.h"

#include "lines_query.h"

namespace ergo::studio::plugins::log {
namespace {
const char* const kEngine = "engine";
const char* const kUi     = "ui";

JsonValue app_names(const std::vector<std::string>& names) {
    JsonValue a = JsonValue::make_array();
    for (const auto& n : names) a.push(jstr(n));
    return a;
}

JsonValue line_array(const std::vector<StoredLine>& lines) {
    JsonValue a = JsonValue::make_array();
    for (const auto& l : lines) a.push(to_json_value(l));
    return a;
}

JsonValue app_array(const std::vector<AppInfo>& apps) {
    JsonValue a = JsonValue::make_array();
    for (const auto& x : apps) a.push(to_json_value(x));
    return a;
}
} // namespace

LogPlugin::LogPlugin(host::LogFn log, std::size_t ring_capacity)
    : info_{"log", "Log", "Live ergo_log viewer \xC2\xB7 per-app ring buffers \xC2\xB7 level / text / frame filters",
            "\xF0\x9F\x93\x9C"},
      log_(std::move(log)),
      apps_(ring_capacity) {}

net::HttpResponse LogPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/apps") return json_response(200, jobj({{"apps", app_array(apps())}}));
    if (sub != "/api/lines") return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");

    std::lock_guard<std::mutex> lk(mtx_);
    const auto app = req.query.find("app");
    if (app == req.query.end() || app->second.empty()) {
        return json_response(400, jobj({{"error", jstr("app is required")}, {"apps", app_names(apps_.names())}}));
    }
    if (!apps_.has(app->second)) {
        return json_response(404, jobj({{"error", jstr("unknown app \"" + app->second + "\"")},
                                        {"apps", app_names(apps_.names())}}));
    }
    const auto parsed = parse_lines_query(req.query);
    if (const auto* err = std::get_if<std::string>(&parsed)) return json_response(400, jobj({{"error", jstr(*err)}}));
    const auto& q = std::get<LinesQuery>(parsed);
    const auto  r = apps_.query(app->second, q.filter, q.limit);
    return json_response(200, jobj({{"app", jstr(r->app)}, {"last_seq", jnum(static_cast<double>(r->last_seq))},
                                    {"dropped", jnum(static_cast<double>(r->dropped))}, {"lines", line_array(r->lines)}}));
}

net::WsHandlers LogPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    {
        std::lock_guard<std::mutex> lk(mtx_);
        conns_[id] = conn;
    }
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text  = [this, id](const std::string& t) { on_text(id, t); };
    h.on_close = [this, id] { on_close(id); };
    return h;
}

void LogPlugin::on_text(uint64_t id, const std::string& text) {
    const auto msg = parse_message(text);
    if (!msg) return;
    const std::string op   = op_of(*msg);
    const std::string role = clients_.role(id);

    if (op == "hello") {
        const JsonValue*  r     = field(*msg, "role");
        const std::string wants = r ? r->as_string() : std::string();
        if (wants == kEngine && role.empty()) {
            const JsonValue* app = field(*msg, "app");
            become_engine(id, app ? app->as_string() : std::string());
        } else if (wants == kUi && role != kEngine) {
            clients_.set_role(id, kUi);
            std::vector<std::string> frames;
            {
                std::lock_guard<std::mutex> lk(mtx_);
                frames.push_back(to_json(jobj({{"op", jstr("apps")}, {"apps", app_array(apps_.list())}})));
                for (const auto& name : apps_.names()) {
                    const auto r2 = apps_.query(name);
                    frames.push_back(to_json(jobj({{"op", jstr("backlog")}, {"app", jstr(name)},
                                                   {"dropped", jnum(static_cast<double>(r2->dropped))},
                                                   {"lines", line_array(r2->lines)}})));
                }
            }
            for (const auto& f : frames) clients_.send(id, f);
        }
        return;
    }

    if (op != "lines") return;
    const auto body = sanitize_lines_msg(*msg);
    if (!body) return;
    // After a reconnect the first batch can overtake hello: the frame's own app names the engine.
    if (role.empty() && !become_engine(id, body->app)) return;
    if (clients_.role(id) != kEngine) return;

    std::string frame;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        const auto it = engine_app_.find(id);
        if (it == engine_app_.end()) return;
        const auto stored = apps_.ingest(it->second, body->dropped, body->lines);
        if (stored.empty() && body->dropped == 0) return;
        frame = to_json(jobj({{"op", jstr("lines")}, {"app", jstr(it->second)},
                              {"dropped", jnum(static_cast<double>(body->dropped))}, {"lines", line_array(stored)}}));
    }
    ++revision_;
    clients_.broadcast(frame, kUi);
}

bool LogPlugin::become_engine(uint64_t id, const std::string& requested) {
    std::string                         name;
    std::shared_ptr<net::IWsConnection> refused;
    {
        // Count and claim under one lock so two engines racing in cannot both pass the limit.
        // (ClientSet never takes mtx_, so the nesting cannot deadlock.)
        std::lock_guard<std::mutex> lk(mtx_);
        if (engine_app_.size() >= kMaxEngines) {
            const auto it = conns_.find(id);
            if (it != conns_.end()) refused = it->second;
        } else {
            clients_.set_role(id, kEngine);
            name            = apps_.claim(requested, id);
            engine_app_[id] = name;
        }
    }
    if (name.empty()) {
        if (refused && refused->is_open()) {
            if (log_) log_("warn", "[log] engine refused: " + std::to_string(kMaxEngines) + " engines already connected");
            refused->close(1013);
        }
        return false;
    }
    if (log_) log_("info", "[log] engine connected: " + name);
    ++revision_;
    broadcast_apps();
    return true;
}

void LogPlugin::on_close(uint64_t id) {
    const bool was_engine = clients_.role(id) == kEngine;
    clients_.remove(id);
    std::string released;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        conns_.erase(id);
        const auto it = engine_app_.find(id);
        if (it != engine_app_.end()) {
            if (was_engine && apps_.release(it->second, id)) released = it->second;
            engine_app_.erase(it);
        }
    }
    if (released.empty()) return;
    if (log_) log_("info", "[log] engine disconnected: " + released);
    ++revision_;
    broadcast_apps();
}

void LogPlugin::broadcast_apps() {
    std::string frame;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        frame = to_json(jobj({{"op", jstr("apps")}, {"apps", app_array(apps_.list())}}));
    }
    clients_.broadcast(frame, kUi);
}

JsonValue LogPlugin::snapshot() const {
    std::size_t apps = 0;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        apps = apps_.size();
    }
    // `clients` is the field the shell's sidebar badge reads.
    return jobj({{"ok", jbool(true)}, {"clients", jnum(static_cast<double>(clients_.size()))},
                 {"apps", jnum(static_cast<double>(apps))},
                 {"engines", jnum(static_cast<double>(clients_.count(kEngine)))},
                 {"uis", jnum(static_cast<double>(clients_.count(kUi)))}});
}

std::vector<AppInfo> LogPlugin::apps() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return apps_.list();
}

std::optional<LogApps::Lines> LogPlugin::lines(const std::string& app, const LineFilter& f, std::size_t limit) const {
    std::lock_guard<std::mutex> lk(mtx_);
    return apps_.query(app, f, limit);
}

std::unique_ptr<host::IPlugin> make_log_plugin(const host::LogFn& log) { return std::make_unique<LogPlugin>(log); }

} // namespace ergo::studio::plugins::log
