#include "tuning_plugin.h"

#include "../common/json_parse.h"
#include "../studio/endpoints.h"

namespace ergo::studio::plugins::tuning {
namespace fs = std::filesystem;

namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

std::string text_of(const JsonValue& v, const char* key) {
    const JsonValue* f = field(v, key);
    return f != nullptr && f->is_string() ? f->as_string() : std::string();
}

/// "app=amicii&x=1" -> the value of `key` (no percent-decoding: app names are plain).
std::string query_value(const std::string& query, const std::string& key) {
    std::size_t start = 0;
    while (start < query.size()) {
        const std::size_t end = query.find('&', start);
        const std::string pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (pair.rfind(key + "=", 0) == 0) return pair.substr(key.size() + 1);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

} // namespace

TuningPlugin::TuningPlugin(TuningStore store, std::string home, std::string plugin_dirs, host::LogFn log)
    : info_{"tuning", "調整値", "ゲームが ergo_bind で出す値をカテゴリごとに調整し、名前とメモ付きの版として保存・比較・復元する。",
            "\xF0\x9F\x8E\x9A"},
      store_(std::move(store)),
      home_(std::move(home)),
      plugin_dirs_(std::move(plugin_dirs)),
      log_(std::move(log)) {}

fs::path TuningPlugin::folder_of(const std::string& app) const {
    fs::path profile_dir;
    for (const auto& p : load_workspace_profiles(fs::u8path(home_), plugin_dirs_, &process_env).profiles) {
        if (const auto it = p.tuning.find(app); it != p.tuning.end()) profile_dir = it->second;
    }
    return store_.folder(app, profile_dir);
}

JsonValue TuningPlugin::snapshot() const {
    const ProfileSet set = load_workspace_profiles(fs::u8path(home_), plugin_dirs_, &process_env);
    JsonValue folders = JsonValue::make_object();
    for (const auto& p : set.profiles) {
        for (const auto& [app, dir] : p.tuning) folders.set(app, jstr(dir.u8string()));
    }
    JsonValue errors = JsonValue::make_array();
    for (const auto& e : set.errors) errors.push(jstr(e));
    JsonValue out = jobj({{"ok", jbool(!home_.empty() || !folders.o->empty())}, {"folders", folders}, {"profile_errors", errors}});
    if (home_.empty() && folders.o->empty()) out.set("err", jstr("the Studio home is unknown and no profile names a folder"));
    return out;
}

net::HttpResponse TuningPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const std::size_t q = sub.find('?');
    const std::string path = sub.substr(0, q);
    const std::string query = q == std::string::npos ? std::string() : sub.substr(q + 1);
    if (path == "/api/health") return json_response(200, snapshot());
    if (path == "/api/versions" && req.method == "GET") {
        const std::string app = query_value(query, "app");
        if (!safe_part(app)) return fail(400, "app is required");
        const fs::path folder = folder_of(app);
        JsonValue list = JsonValue::make_array();
        for (const auto& v : store_.list(folder)) {
            list.push(jobj({{"id", jstr(v.id)},
                            {"name", jstr(v.name)},
                            {"note", jstr(v.note)},
                            {"created", jstr(v.created)},
                            {"values", jnum(static_cast<double>(v.values))}}));
        }
        return json_response(200, jobj({{"folder", jstr(folder.u8string())}, {"versions", list}}));
    }
    if (path == "/api/versions" && req.method == "POST") return save(req);
    const std::string prefix = "/api/versions/";
    if (path.rfind(prefix, 0) == 0 && req.method == "GET") {
        const std::string rest = path.substr(prefix.size());
        const std::size_t slash = rest.find('/');
        if (slash == std::string::npos) return fail(400, "expected /api/versions/<app>/<id>");
        const std::string app = rest.substr(0, slash), id = rest.substr(slash + 1);
        if (!safe_part(app)) return fail(400, "bad app");
        std::string err;
        const auto version = store_.load(folder_of(app), id, err);
        if (!version) return fail(404, err);
        return json_response(200, *version);
    }
    return fail(404, "not found");
}

net::HttpResponse TuningPlugin::save(const net::HttpRequest& req) {
    const auto body = parse_json(req.body);
    if (!body) return fail(400, "body must be JSON");
    const std::string app = text_of(*body, "app");
    if (!safe_part(app)) return fail(400, "app is required");
    const JsonValue* values = field(*body, "values");
    std::string err;
    const auto id = store_.save(folder_of(app), app, text_of(*body, "name"), text_of(*body, "note"),
                                values != nullptr ? *values : JsonValue::make_object(), studio::now_iso_utc(), err);
    if (!id) return fail(400, err);
    if (log_) log_("info", "[tuning] " + app + " version saved: " + *id);
    return json_response(200, jobj({{"ok", jbool(true)}, {"id", jstr(*id)}}));
}

std::unique_ptr<host::IPlugin> make_tuning_plugin(const std::string& studio_home, const std::string& plugin_dirs,
                                                  const host::LogFn& log) {
    const fs::path home = fs::u8path(studio_home);
    return std::make_unique<TuningPlugin>(TuningStore(home.empty() ? fs::path() : home / "tuning"), studio_home, plugin_dirs, log);
}

} // namespace ergo::studio::plugins::tuning
