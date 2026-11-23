#include "web_plugin.h"

#include "../common/json_util.h"

#include <algorithm>
#include <cctype>

// The page, compiled in by ergo_embed_text (CMakeLists.txt).
extern const char        ergo_web_index_html[];
extern const std::size_t ergo_web_index_html_size;
extern const char        ergo_web_app_js[];
extern const std::size_t ergo_web_app_js_size;
extern const char        ergo_web_style_css[];
extern const std::size_t ergo_web_style_css_size;

namespace ergo::studio::plugins::web {

std::string url_origin(const std::string& url) {
    std::string lower = url;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::size_t scheme_end = 0;
    if (lower.rfind("https://", 0) == 0) scheme_end = 8;
    else if (lower.rfind("http://", 0) == 0) scheme_end = 7;
    else return {};
    const std::size_t end = lower.find_first_of("/?#", scheme_end);
    const std::string authority = lower.substr(scheme_end, end == std::string::npos ? std::string::npos : end - scheme_end);
    if (authority.empty() || authority.find('@') != std::string::npos) return {};
    const bool ok = std::all_of(authority.begin(), authority.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '-' || c == ':' || c == '[' || c == ']';
    });
    if (!ok) return {};
    return lower.substr(0, scheme_end) + authority;
}

std::string page_csp(const std::string& play_url) {
    const std::string frame = url_origin(play_url);
    return "default-src 'self'; img-src 'self' data: blob:; connect-src 'self'; style-src 'self'; script-src 'self'; "
           "frame-src " + (frame.empty() ? std::string("'none'") : frame) + "; frame-ancestors 'none'; base-uri 'none'; "
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
        res.headers["content-security-policy"] = page_csp(cfg_.play_url);
        res.headers["x-frame-options"] = "DENY";
        return res;
    }
    if (sub == "/app.js") return page_file(ergo_web_app_js, ergo_web_app_js_size, "text/javascript; charset=utf-8");
    if (sub == "/style.css") return page_file(ergo_web_style_css, ergo_web_style_css_size, "text/css; charset=utf-8");
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/config") {
        JsonValue hosts = JsonValue::make_array();
        for (const auto& h : cfg_.public_hosts) hosts.push(jstr(h));
        return json_response(200, jobj({{"play_url", jstr(url_origin(cfg_.play_url).empty() ? std::string() : cfg_.play_url)},
                                        {"public_hosts", hosts}}));
    }
    return net::HttpResponse::json(404, "{\"ok\":false,\"err\":\"not found\"}");
}

JsonValue WebPlugin::snapshot() const {
    JsonValue out = jobj({{"ok", jbool(true)},
                          {"play_url", jbool(!url_origin(cfg_.play_url).empty())},
                          {"public_hosts", jnum(static_cast<double>(cfg_.public_hosts.size()))}});
    if (url_origin(cfg_.play_url).empty()) out.set("note", jstr("ERGO_STUDIO_PLAY_URL is not set: the game tab shows the services only"));
    return out;
}

std::unique_ptr<host::IPlugin> make_web_plugin(const WebConfig& cfg, const host::LogFn&) {
    return std::make_unique<WebPlugin>(cfg);
}

} // namespace ergo::studio::plugins::web
