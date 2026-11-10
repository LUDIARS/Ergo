#include "render_pipeline_plugin.h"

#include "profile_schema.h"

namespace ergo::studio::plugins::render_pipeline {
namespace {

constexpr int     kSchemaVersion = 2;
const char* const kNoDir         = "ERGO_PICTOR_PROFILE_DIR is not set (the directory of Pictor *.profile.json)";

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

} // namespace

RenderPipelinePlugin::RenderPipelinePlugin(std::filesystem::path profile_dir, host::LogFn log)
    : info_{"render_pipeline", "Render Pipeline", "Pictor の *.profile.json を単一グラフで編集 (Phase 3)。",
            "\xF0\x9F\x94\xB2"},
      store_(std::move(profile_dir)),
      log_(std::move(log)) {}

net::HttpResponse RenderPipelinePlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    const bool get = req.method == "GET";
    if (sub == "/api/health" && get) return json_response(200, snapshot());
    if (!store_.configured()) return fail(503, kNoDir);
    if (sub == "/api/profiles" && get) {
        JsonValue list = JsonValue::make_array();
        for (const auto& e : store_.list()) list.push(to_json_value(e));
        return json_response(200, jobj({{"ok", jbool(true)}, {"dir", jstr(store_.dir().u8string())},
                                        {"version", jnum(kProfileSchemaVersion)}, {"profiles", list}}));
    }
    const std::string prefix = "/api/profile/";
    if (get && sub.compare(0, prefix.size(), prefix) == 0) {
        const auto r = store_.load(sub.substr(prefix.size()));
        if (!r.ok) return fail(r.status, r.err);
        return json_response(200, jobj({{"ok", jbool(true)}, {"file", jstr(r.file)}, {"profile", r.profile},
                                        {"mtime", jstr(r.mtime)}}));
    }
    if (sub == "/api/profile" && req.method == "POST") {
        JsonValue body;
        if (!ergo::common::jsonm::parse(req.body, body) || !field(body, "profile")) {
            return fail(400, "body must be { profile, file? }");
        }
        const JsonValue* f = field(body, "file");
        const auto r = save(*field(body, "profile"), f && f->is_string() ? f->s : std::string());
        if (!r.ok) return fail(r.status == 500 ? 500 : 400, r.err);
        return json_response(200, jobj({{"ok", jbool(true)}, {"file", jstr(r.file)}, {"profile", r.profile},
                                        {"mtime", jstr(r.mtime)}}));
    }
    return fail(404, "not found");
}

ProfileStore::Result RenderPipelinePlugin::save(const JsonValue& profile, const std::string& file) {
    if (!store_.configured()) {
        ProfileStore::Result r;
        r.status = 503;
        r.err    = kNoDir;
        return r;
    }
    auto r = store_.save(profile, file);
    if (!r.ok) return r;
    if (log_) log_("info", "[render_pipeline] saved profile " + r.file);
    ++revision_;
    clients_.broadcast(to_json(jobj({{"op", jstr("profiles-changed")}, {"file", jstr(r.file)}})));
    return r;
}

net::WsHandlers RenderPipelinePlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    clients_.add(std::move(conn));
    net::WsHandlers h;
    h.on_text = [this, id](const std::string& text) {
        const auto msg = parse_message(text);
        if (!msg) return;
        const std::string op = op_of(*msg);
        if (op == "ping") {
            clients_.send(id, "{\"op\":\"ack\"}");
        } else if (op == "timing") {
            {
                std::lock_guard<std::mutex> lk(mtx_);
                last_timing_ = *msg;
            }
            ++revision_;
            clients_.broadcast(text);   // relayed as received (GraphView colours nodes by it)
        }
    };
    h.on_close = [this, id] { clients_.remove(id); };
    return h;
}

JsonValue RenderPipelinePlugin::last_timing() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return deep_copy(last_timing_);
}

JsonValue RenderPipelinePlugin::snapshot() const {
    JsonValue h = jobj({{"ok", jbool(true)}, {"version", jnum(kSchemaVersion)},
                        {"clients", jnum(static_cast<double>(clients_.size()))},
                        {"profile_schema_version", jnum(kProfileSchemaVersion)},
                        {"profile_dir", jstr(store_.dir().u8string())},
                        {"profile_count", jnum(static_cast<double>(store_.list().size()))}});
    if (!store_.configured()) h.set("profile_dir_err", jstr(kNoDir));
    return h;
}

std::unique_ptr<host::IPlugin> make_render_pipeline_plugin(const std::string& profile_dir_env, const host::LogFn& log) {
    std::filesystem::path dir;
    if (profile_dir_env.find_first_not_of(" \t") != std::string::npos) {
        std::error_code ec;
        dir = std::filesystem::absolute(std::filesystem::u8path(profile_dir_env), ec).lexically_normal();
    }
    return std::make_unique<RenderPipelinePlugin>(std::move(dir), log);
}

} // namespace ergo::studio::plugins::render_pipeline
