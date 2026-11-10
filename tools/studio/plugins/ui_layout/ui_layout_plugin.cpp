#include "ui_layout_plugin.h"

#include "../common/file_io.h"
#include "../common/safe_path.h"

namespace ergo::studio::plugins::ui_layout {
namespace {

const char* const kLayoutExt = ".uilayout.json";

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"error", jstr(err)}}));
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

bool is_http_url(const std::string& url) {
    for (const char* scheme : {"http://", "https://"}) {
        const std::string s(scheme);
        if (url.compare(0, s.size(), s) == 0 && url.size() > s.size() && url[s.size()] != '/') return true;
    }
    return false;
}

UiLayoutPlugin::UiLayoutPlugin(std::filesystem::path root, std::shared_ptr<platform::IHttpClient> http, host::LogFn log)
    : info_{"ui_layout", "UI Layout",
            "Figma-like UI layout editor with canvas, hierarchy, properties, file I/O and custos bridge.", "\xF0\x9F\xA7\xA9"},
      root_(std::move(root)),
      http_(std::move(http)),
      log_(std::move(log)) {}

JsonValue UiLayoutPlugin::bridge_json() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"patchUrl", jstr(patch_url_)}, {"pullUrl", jstr(pull_url_)}});
}

net::HttpResponse UiLayoutPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get  = req.method == "GET";
    const bool post = req.method == "POST";
    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (sub == "/api/bridge/config" && get) return json_response(200, bridge_json());
    if (sub == "/api/bridge/config" && post) {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) return fail(400, "invalid JSON body");
        const JsonValue* patch = field(body, "patchUrl");
        const JsonValue* pull  = field(body, "pullUrl");
        for (const JsonValue* u : {patch, pull}) {
            if (u && u->is_string() && !u->s.empty() && !is_http_url(u->s)) return fail(400, "bridge URLs must be http(s)://host/...");
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            if (patch && patch->is_string() && !patch->s.empty()) patch_url_ = patch->s;
            if (pull && pull->is_string() && !pull->s.empty()) pull_url_ = pull->s;
        }
        JsonValue out = bridge_json();
        out.set("ok", jbool(true));
        return json_response(200, out);
    }
    if (sub == "/api/file/open" && get) return open_file(req, kLayoutExt, false);
    if (sub == "/api/file/svg" && get) return open_file(req, ".svg", true);
    if (sub == "/api/file/save" && post) return save_file(req);
    if (sub == "/api/bridge/patch" && post) return bridge_patch(req);
    if (sub == "/api/bridge/pull" && post) return bridge_pull();
    return fail(404, "not found");
}

net::HttpResponse UiLayoutPlugin::open_file(const net::HttpRequest& req, const char* ext, bool svg) {
    if (root_.empty()) return fail(503, "ui_layout file root is not configured");
    const auto it = req.query.find("path");
    const std::string rel = it == req.query.end() ? std::string() : it->second;
    if (rel.find_first_not_of(" \t") == std::string::npos) return fail(400, "path is empty");
    const auto full = resolve_within(root_, rel);
    if (!full) return fail(400, "path must stay within the tool root");
    if (!ends_with(full->u8string(), ext)) return fail(400, std::string("file extension must be ") + ext);
    std::string err;
    const auto text = read_text_file(*full, err);
    if (!text) return fail(400, err);
    if (svg) {
        net::HttpResponse r;
        r.status                  = 200;
        r.headers["content-type"] = "image/svg+xml; charset=utf-8";
        r.body                    = *text;
        return r;
    }
    return json_response(200, jobj({{"ok", jbool(true)}, {"path", jstr(full->u8string())}, {"json", jstr(*text)}}));
}

net::HttpResponse UiLayoutPlugin::save_file(const net::HttpRequest& req) {
    if (root_.empty()) return fail(503, "ui_layout file root is not configured");
    JsonValue body;
    if (!ergo::common::jsonm::parse(req.body, body) || !body.is_object()) return fail(400, "invalid JSON body");
    const JsonValue* path = field(body, "path");
    const JsonValue* json = field(body, "json");
    if (!path || !path->is_string() || path->s.find_first_not_of(" \t") == std::string::npos) return fail(400, "path is empty");
    if (!json || !json->is_string()) return fail(400, "json must be a string");
    const auto full = resolve_within(root_, path->s);
    if (!full) return fail(400, "path must stay within the tool root");
    if (!ends_with(full->u8string(), kLayoutExt)) return fail(400, std::string("file extension must be ") + kLayoutExt);
    std::string err;
    if (!write_text_file(*full, json->s, err)) return fail(400, err);
    return json_response(200, jobj({{"ok", jbool(true)}, {"path", jstr(full->u8string())}}));
}

net::HttpResponse UiLayoutPlugin::bridge_patch(const net::HttpRequest& req) {
    std::string url;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        url = patch_url_;
    }
    if (url.empty()) return fail(503, "bridge patchUrl is not configured");
    platform::HttpClientRequest out;
    out.method  = "POST";
    out.url     = url;
    out.headers = {{"Content-Type", "application/json"}};
    out.body    = req.body;
    const auto res = http_->send(out);
    broadcast("patch", req.body);
    return json_response(200, jobj({{"ok", jbool(res.ok())}, {"status", jnum(res.status)}}));
}

net::HttpResponse UiLayoutPlugin::bridge_pull() {
    std::string url;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        url = pull_url_;
    }
    if (url.empty()) return fail(503, "bridge pullUrl is not configured");
    platform::HttpClientRequest out;
    out.url = url;
    const auto res = http_->send(out);
    if (!res.ok()) return json_response(502, jobj({{"ok", jbool(false)}, {"status", jnum(res.status)}}));
    broadcast("document", res.body);
    return json_response(200, jobj({{"ok", jbool(true)}, {"payload", jstr(res.body)}}));
}

void UiLayoutPlugin::broadcast(const std::string& op, const std::string& payload) {
    clients_.broadcast(to_json(jobj({{"op", jstr(op)}, {"payload", jstr(payload)}})));
}

net::WsHandlers UiLayoutPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text = [this](const std::string& text) {
        const auto msg = parse_message(text);
        if (!msg) return;
        const std::string op = op_of(*msg);
        // Relayed verbatim, as the TS plugin did.
        if (op == "patch" || op == "document") clients_.broadcast(text);
    };
    h.on_close = [this, id] { clients_.remove(id); };
    return h;
}

JsonValue UiLayoutPlugin::snapshot() const {
    JsonValue h = bridge_json();
    h.set("ok", jbool(true));
    h.set("clients", jnum(static_cast<double>(clients_.size())));
    h.set("file_root", jstr(root_.u8string()));
    return h;
}

std::unique_ptr<host::IPlugin> make_ui_layout_plugin(const std::filesystem::path& root, const host::LogFn& log) {
    return std::make_unique<UiLayoutPlugin>(root, platform::make_system_http_client(), log);
}

} // namespace ergo::studio::plugins::ui_layout
