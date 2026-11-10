#include "visus_plugin.h"

#include <cstdlib>

namespace ergo::studio::plugins::visus {
namespace {

constexpr int kSchemaVersion = 1;
const char* const kNoRoots = "VISUS_PROJECT_ROOTS is not set (\";\"-separated project roots)";

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

/// Number of a query value; NaN when absent or not a number (rejected by root_at).
double query_index(const net::HttpRequest& req) {
    const auto it = req.query.find("root");
    const std::string raw = it == req.query.end() ? "0" : it->second;
    char* end = nullptr;
    const double v = std::strtod(raw.c_str(), &end);
    return end == raw.c_str() + raw.size() && !raw.empty() ? v : std::nan("");
}

} // namespace

VisusPlugin::VisusPlugin(std::vector<std::filesystem::path> roots, host::LogFn log)
    : info_{"visus", "Visus Editor",
            "各プロジェクトの assets/visus/*.visus.json を scan + 編集 (Pictor 描画定義)。", "\xF0\x9F\x91\x81"},
      log_(std::move(log)),
      store_(std::move(roots)) {}

net::HttpResponse VisusPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health" && req.method == "GET") return json_response(200, snapshot());
    if (sub == "/api/roots" && req.method == "GET") {
        JsonValue arr = JsonValue::make_array();
        for (std::size_t i = 0; i < store_.roots().size(); ++i) {
            arr.push(jobj({{"index", jnum(static_cast<double>(i))}, {"path", jstr(store_.roots()[i].u8string())}}));
        }
        return json_response(200, jobj({{"roots", arr}}));
    }
    if (store_.roots().empty() && (sub == "/api/scan" || sub == "/api/load" || sub == "/api/save")) {
        return fail(503, kNoRoots);
    }
    if (sub == "/api/scan" && req.method == "GET") {
        const auto* root = store_.root_at(query_index(req));
        if (!root) return fail(400, "invalid root index");
        std::error_code ec;
        if (!std::filesystem::exists(*root, ec)) return fail(404, "root not found: " + root->u8string());
        JsonValue files = JsonValue::make_array();
        for (const auto& s : store_.scan(*root)) files.push(to_json_value(s));
        return json_response(200, jobj({{"ok", jbool(true)}, {"root", jstr(root->u8string())}, {"files", files}}));
    }
    if (sub == "/api/load" && req.method == "GET") {
        const auto it  = req.query.find("path");
        const double idx = query_index(req);
        const auto r   = store_.load(idx, it == req.query.end() ? std::string() : it->second);
        if (!r.ok) return fail(r.status, r.err);
        return json_response(200, jobj({{"ok", jbool(true)}, {"root_index", jnum(idx)},
                                        {"rel_path", jstr(it->second)}, {"content", jstr(r.content)}}));
    }
    if (sub == "/api/save" && req.method == "POST") {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) return fail(400, "invalid JSON body");
        const JsonValue* root    = body.find("root");
        const JsonValue* path    = body.find("path");
        const JsonValue* content = body.find("content");
        const auto r = save(root ? root->as_number(0) : 0, path ? path->as_string() : std::string(),
                            content ? content->as_string() : std::string());
        if (!r.ok) return fail(r.status, r.err);
        return json_response(200, jobj({{"ok", jbool(true)}}));
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

VisusStore::Result VisusPlugin::save(double root_index, const std::string& rel, const std::string& content) {
    if (store_.roots().empty()) return {false, 503, kNoRoots, {}};
    auto r = store_.save(root_index, rel, content);
    if (!r.ok) return r;
    if (log_) log_("info", "[visus] saved " + rel);
    ++revision_;
    clients_.broadcast(to_json(jobj({{"op", jstr("saved")}, {"root_index", jnum(root_index)}, {"rel_path", jstr(rel)}})));
    return r;
}

net::WsHandlers VisusPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text = [this, id](const std::string& text) {
        const auto msg = parse_message(text);
        if (msg && op_of(*msg) == "ping") clients_.send(id, "{\"op\":\"ack\"}");
    };
    h.on_close = [this, id] { clients_.remove(id); };
    return h;
}

JsonValue VisusPlugin::snapshot() const {
    JsonValue h = jobj({{"ok", jbool(true)}, {"version", jnum(kSchemaVersion)},
                        {"roots", jnum(static_cast<double>(store_.roots().size()))},
                        {"clients", jnum(static_cast<double>(clients_.size()))}});
    if (store_.roots().empty()) h.set("warning", jstr(kNoRoots));
    return h;
}

std::unique_ptr<host::IPlugin> make_visus_plugin(const std::string& roots_env, const host::LogFn& log) {
    return std::make_unique<VisusPlugin>(parse_roots(roots_env), log);
}

} // namespace ergo::studio::plugins::visus
