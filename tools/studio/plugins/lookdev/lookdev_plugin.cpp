#include "lookdev_plugin.h"

#include "../common/json_util.h"

#include <filesystem>

namespace ergo::studio::plugins::lookdev {
namespace fs = std::filesystem;

namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string utf8_text(const fs::path& p) { return p.u8string(); }

fs::path utf8_path(const std::string& text) { return fs::u8path(text); }

std::string string_field(const JsonValue& body, const char* key) {
    const JsonValue* v = field(body, key);
    return v != nullptr ? v->as_string() : std::string();
}

} // namespace

LookdevPlugin::LookdevPlugin(LookdevStore store, studio::StudioConfig cfg, std::shared_ptr<platform::IHttpClient> http,
                             host::LogFn log)
    : info_{"lookdev", "\xE3\x83\xAB\xE3\x83\x83\xE3\x82\xAF\xE3\x83\x87\xE3\x83\xB4",   // ルックデヴ
            "\xE3\x82\xB2\xE3\x83\xBC\xE3\x83\xA0\xE3\x81\xAE\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\x86\xE3\x82\xA3\xE3\x83\xB3\xE3\x82\xB0\xE3\x82\x92\xE9\x81\x8A\xE3\x81\xB3\xE3\x81\xAA\xE3\x81\x8C\xE3\x82\x89\xE8\xAA\xBF\xE6\x95\xB4\xE3\x81\x97\xE3\x80\x81\xE7\x94\xBB\xE9\x9D\xA2\xE3\x81\xA8\xE8\xA8\xAD\xE5\xAE\x9A\xE3\x82\x92 Discord / Slack \xE3\x81\xAB\xE6\x8A\x95\xE7\xA8\xBF\xE3\x81\x99\xE3\x82\x8B\xE3\x80\x82",   // ゲームのライティングを遊びながら調整し、画面と設定を Discord / Slack に投稿する。
            "\xF0\x9F\x92\xA1"},
      store_(std::move(store)),
      cfg_(std::move(cfg)),
      caller_(std::make_unique<studio::ServiceCaller>(std::move(http), cfg_.request_timeout_ms)),
      endpoints_(std::make_unique<studio::EndpointResolver>(cfg_, *caller_)),
      poster_(*endpoints_, *caller_),
      log_(std::move(log)) {}

JsonValue LookdevPlugin::snapshot() const {
    const bool ready = !store_.presets_dir().empty() && !store_.captures_root().empty();
    JsonValue out = jobj({{"ok", jbool(ready)},
                          {"presets_dir", jstr(utf8_text(store_.presets_dir()))},
                          {"captures_dir", jstr(utf8_text(store_.captures_root()))}});
    if (!ready) out.set("err", jstr("the Studio home or the temp directory is unknown"));
    return out;
}

net::HttpResponse LookdevPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/presets" && req.method == "GET") {
        JsonValue list = JsonValue::make_array();
        for (const auto& entry : store_.presets()) {
            list.push(jobj({{"name", jstr(entry.name)}, {"updated", jstr(entry.updated)}}));
        }
        return json_response(200, list);
    }
    if (sub == "/api/presets" && req.method == "POST") return save_preset(req);
    if (sub.rfind("/api/presets/", 0) == 0 && req.method == "GET") {
        const std::string name = sub.substr(std::string("/api/presets/").size());
        std::string err;
        const auto text = store_.load(name, err);
        if (!text) return fail(404, err);
        return json_response(200, jobj({{"name", jstr(name)}, {"text", jstr(*text)}}));
    }
    if (sub == "/api/capture-dir" && req.method == "POST") return capture_dir();
    if (sub == "/api/post" && req.method == "POST") return post_capture(req);
    return fail(404, "not found");
}

net::HttpResponse LookdevPlugin::save_preset(const net::HttpRequest& req) {
    const auto body = parse_message(req.body);
    if (!body) return fail(400, "body must be JSON");
    const std::string name = string_field(*body, "name");
    std::string err;
    if (!store_.save(name, string_field(*body, "text"), err)) return fail(400, err);
    if (log_) log_("info", "[lookdev] preset saved: " + name);
    return json_response(200, jobj({{"ok", jbool(true)}, {"name", jstr(name)}}));
}

net::HttpResponse LookdevPlugin::capture_dir() {
    std::string err;
    const auto dir = store_.new_capture_dir(err);
    if (!dir) return fail(500, err);
    return json_response(200, jobj({{"ok", jbool(true)}, {"dir", jstr(utf8_text(*dir))}}));
}

net::HttpResponse LookdevPlugin::post_capture(const net::HttpRequest& req) {
    const auto body = parse_message(req.body);
    if (!body) return fail(400, "body must be JSON");
    const fs::path png = utf8_path(string_field(*body, "png"));
    // Only stills the Studio asked for: a request cannot make the Studio attach any file.
    if (!store_.is_capture(png) || png.extension() != ".png" || !fs::is_regular_file(png)) {
        return fail(400, "png must be a still in the captures folder");
    }
    std::vector<std::string> attachments{utf8_text(png)};
    fs::path preset = png;
    preset.replace_extension(".look");
    if (fs::is_regular_file(preset)) attachments.push_back(utf8_text(preset));

    std::vector<std::string> changes;
    if (const JsonValue* list = field(*body, "changes"); list != nullptr && list->is_array()) {
        for (const auto& item : *list->a) {
            if (item.is_string()) changes.push_back(item.s);
        }
    }
    const std::string text = compose_post_text(string_field(*body, "app"), string_field(*body, "comment"), changes);
    const studio::ServiceResult result = poster_.post(text, attachments);
    if (!result.ok) {
        if (log_) log_("error", "[lookdev] post failed: " + result.message);
        return fail(502, "Concordia: " + result.message);
    }
    if (log_) log_("info", "[lookdev] posted " + utf8_text(png));
    JsonValue out = jobj({{"ok", jbool(true)}});
    if (const JsonValue* id = field(result.data, "id")) out.set("message_id", *id);
    return json_response(200, out);
}

std::unique_ptr<host::IPlugin> make_lookdev_plugin(const studio::StudioConfig& cfg, const host::LogFn& log) {
    const fs::path home = utf8_path(cfg.home);
    const fs::path temp = long_temp_directory();
    LookdevStore store(home.empty() ? fs::path() : home / "lookdev" / "presets",
                       temp.empty() ? fs::path() : temp / "ergo-studio-lookdev");
    return std::make_unique<LookdevPlugin>(std::move(store), cfg, platform::make_system_http_client(), log);
}

} // namespace ergo::studio::plugins::lookdev
