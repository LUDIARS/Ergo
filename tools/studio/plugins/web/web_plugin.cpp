#include "web_plugin.h"

#include "../common/json_util.h"

// The page, compiled in by ergo_embed_text (CMakeLists.txt).
extern const char        ergo_web_index_html[];
extern const std::size_t ergo_web_index_html_size;
extern const char        ergo_web_scene_js[];
extern const std::size_t ergo_web_scene_js_size;
extern const char        ergo_web_app_js[];
extern const std::size_t ergo_web_app_js_size;
extern const char        ergo_web_style_css[];
extern const std::size_t ergo_web_style_css_size;

namespace ergo::studio::plugins::web {

std::string page_csp() {
    return "default-src 'self'; img-src 'self' data: blob:; media-src 'self' blob:; connect-src 'self'; "
           "style-src 'self'; script-src 'self'; frame-src 'none'; frame-ancestors 'none'; base-uri 'none'; "
           "form-action 'none'";
}

WebPlugin::WebPlugin(WebConfig cfg)
    : info_{"web", "Web", "ブラウザで開く Studio: アセット一覧・ライティング・調整値・ゲーム (起動と遠隔プレイ)。",
            "\xF0\x9F\x8C\x90", "browser: http://{addr}/web/"},
      cfg_(std::move(cfg)) {}

net::HttpResponse WebPlugin::page_file(const char* body, std::size_t size, const char* type) const {
    net::HttpResponse res;
    res.status = 200;
    res.body.assign(body, size);
    res.headers["content-type"] = type;
    res.headers["x-content-type-options"] = "nosniff";
    res.headers["cache-control"] = "no-cache";
    res.headers["referrer-policy"] = "no-referrer";
    return res;
}

net::HttpResponse WebPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (req.method != "GET") return net::HttpResponse::json(405, "{\"ok\":false,\"err\":\"method not allowed\"}");
    if (sub == "/" || sub == "/index.html") {
        auto res = page_file(ergo_web_index_html, ergo_web_index_html_size, "text/html; charset=utf-8");
        res.headers["content-security-policy"] = page_csp();
        res.headers["x-frame-options"] = "DENY";
        return res;
    }
    if (sub == "/scene.js") return page_file(ergo_web_scene_js, ergo_web_scene_js_size, "text/javascript; charset=utf-8");
    if (sub == "/app.js") return page_file(ergo_web_app_js, ergo_web_app_js_size, "text/javascript; charset=utf-8");
    if (sub == "/style.css") return page_file(ergo_web_style_css, ergo_web_style_css_size, "text/css; charset=utf-8");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/config") {
        JsonValue hosts = JsonValue::make_array();
        for (const auto& h : cfg_.public_hosts) hosts.push(jstr(h));
        return json_response(200, jobj({{"public_hosts", hosts}}));
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

JsonValue WebPlugin::snapshot() const {
    return jobj({{"ok", jbool(true)}, {"public_hosts", jnum(static_cast<double>(cfg_.public_hosts.size()))}});
}

std::unique_ptr<host::IPlugin> make_web_plugin(const WebConfig& cfg, const host::LogFn&) {
    return std::make_unique<WebPlugin>(cfg);
}

} // namespace ergo::studio::plugins::web
