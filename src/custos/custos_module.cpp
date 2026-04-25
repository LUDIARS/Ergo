#include "ergo/custos/custos_module.h"

#include "http_server.h"
#include "png_writer.h"

#include <cstdio>
#include <memory>
#include <mutex>

namespace ergo::custos {

namespace {

// プロセス全体で 1 個の HTTP server。シングルトンとして扱う (ライフサイクルは
// `start` / `shutdown` で管理)。
struct Module {
    std::unique_ptr<detail::HttpServer> server;
    std::mutex                          mtx;
    ScreenshotProvider                  screenshot;
    KeyInjectHandler                    key_handler;
    bool                                running = false;
};

Module& mod() {
    static Module m;
    return m;
}

// ─── JSON 風キー抽出 (依存ゼロ) ──────────────
//
// `{"code": 87, "down": true}` のような単純フォーマットを最小限パース。
// ネストや配列、エスケープは扱わない (Custos backend が出す JSON のみ想定)。
//
// 失敗時は false を返す。

bool parse_key_body(const std::string& body, int& code_out, bool& down_out) {
    // "code" を探す
    auto find_value = [&](const char* key) -> std::string {
        std::string needle = std::string("\"") + key + "\"";
        auto p = body.find(needle);
        if (p == std::string::npos) return {};
        p = body.find(':', p + needle.size());
        if (p == std::string::npos) return {};
        ++p;
        while (p < body.size() && (body[p] == ' ' || body[p] == '\t')) ++p;
        std::size_t end = p;
        while (end < body.size() && body[end] != ',' && body[end] != '}' &&
               body[end] != ' ' && body[end] != '\n' && body[end] != '\r') ++end;
        return body.substr(p, end - p);
    };

    std::string code_s = find_value("code");
    std::string down_s = find_value("down");
    if (code_s.empty() || down_s.empty()) return false;
    try {
        code_out = std::stoi(code_s);
    } catch (...) { return false; }
    down_out = (down_s == "true" || down_s == "1");
    return true;
}

// ─── ハンドラ ───────────────────────────────

detail::HttpResponse handle_health() {
    return detail::HttpResponse::text(200, "ok");
}

detail::HttpResponse handle_screenshot() {
    auto& m = mod();
    ScreenshotProvider provider;
    {
        std::lock_guard<std::mutex> g(m.mtx);
        provider = m.screenshot;
    }
    if (!provider) return detail::HttpResponse::not_implemented();

    ScreenshotData data;
    bool ok = false;
    try { ok = provider(data); }
    catch (const std::exception& e) {
        return detail::HttpResponse::text(500, std::string("provider exception: ") + e.what());
    } catch (...) {
        return detail::HttpResponse::text(500, "provider exception: unknown");
    }

    if (!ok || data.rgba.empty() || data.width == 0 || data.height == 0) {
        return detail::HttpResponse::text(503, "screenshot not available yet");
    }
    if (data.rgba.size() != std::size_t(data.width) * data.height * 4) {
        return detail::HttpResponse::text(500, "rgba size != width*height*4");
    }

    auto png = detail::encode_png_rgba8(data.rgba.data(), data.width, data.height);
    if (png.empty()) return detail::HttpResponse::text(500, "png encode failed");
    return detail::HttpResponse::png(std::move(png));
}

detail::HttpResponse handle_key(const detail::HttpRequest& req) {
    auto& m = mod();
    KeyInjectHandler handler;
    {
        std::lock_guard<std::mutex> g(m.mtx);
        handler = m.key_handler;
    }
    if (!handler) return detail::HttpResponse::not_implemented();

    int  code = 0;
    bool down = false;
    if (!parse_key_body(req.body, code, down)) {
        return detail::HttpResponse::text(400, "expected JSON body { code: int, down: bool }");
    }
    try { handler(code, down); }
    catch (const std::exception& e) {
        return detail::HttpResponse::text(500, std::string("handler exception: ") + e.what());
    } catch (...) {
        return detail::HttpResponse::text(500, "handler exception: unknown");
    }
    return detail::HttpResponse::text(204, "");
}

detail::HttpResponse dispatch(const detail::HttpRequest& req) {
    if (req.path == "/health" && req.method == "GET")        return handle_health();
    if (req.path == "/screenshot" && req.method == "GET")    return handle_screenshot();
    if (req.path == "/key" && req.method == "POST")          return handle_key(req);
    return detail::HttpResponse::not_found();
}

} // anonymous

// ─── public API ──────────────────────────────

bool start(const StartConfig& cfg) {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    if (m.running) return true;

    m.server = std::make_unique<detail::HttpServer>();
    bool ok = m.server->start(cfg.host, cfg.port, &dispatch);
    if (!ok) {
        m.server.reset();
        return false;
    }
    m.running = true;
    std::fprintf(stderr, "[ergo_custos] listening on %s:%u\n",
                 cfg.host.c_str(), unsigned(m.server->bound_port()));
    return true;
}

void shutdown() {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    if (!m.running) return;
    if (m.server) m.server->shutdown();
    m.server.reset();
    m.screenshot  = nullptr;
    m.key_handler = nullptr;
    m.running = false;
}

bool is_running() {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    return m.running;
}

uint16_t bound_port() {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    return m.server ? m.server->bound_port() : 0;
}

void set_screenshot_provider(ScreenshotProvider provider) {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    m.screenshot = std::move(provider);
}

void set_key_handler(KeyInjectHandler handler) {
    auto& m = mod();
    std::lock_guard<std::mutex> g(m.mtx);
    m.key_handler = std::move(handler);
}

} // namespace ergo::custos
