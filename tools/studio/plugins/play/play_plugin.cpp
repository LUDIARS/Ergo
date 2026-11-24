#include "play_plugin.h"

#include "play_message.h"
#include "../common/workspace_profiles.h"

#include <charconv>
#include <stdexcept>

namespace ergo::studio::plugins::play {
namespace fs = std::filesystem;

namespace {

net::HttpResponse fail(int status, const std::string& err) {
    return json_response(status, jobj({{"ok", jbool(false)}, {"err", jstr(err)}}));
}

JsonValue error_msg(const std::string& error, const std::string& message = {}) {
    JsonValue out = jobj({{"type", jstr("error")}, {"error", jstr(error)}});
    if (!message.empty()) out.set("message", jstr(message));
    return out;
}

JsonValue state_msg(const std::string& target, const TargetStatus& s) {
    JsonValue out = jobj({{"type", jstr("state")}, {"target", jstr(target)}, {"state", jstr(state_name(s.state))}});
    out.set("exit_code", s.exit_code ? jnum(*s.exit_code) : JsonValue::make_null());
    return out;
}

uint64_t parse_since(const net::HttpRequest& req) {
    const auto it = req.query.find("since");
    if (it == req.query.end()) return 0;
    uint64_t value = 0;
    std::from_chars(it->second.data(), it->second.data() + it->second.size(), value);
    return value;
}

} // namespace

PlayPlugin::PlayPlugin(PlayDeps deps, host::LogFn log)
    : info_{"play", "Play",
            "プロフィールの play のゲームをビルド・実行・停止し、窓の画面を WebRTC で送って、キーボードとマウスで操作する。",
            "\xF0\x9F\x8E\xAE"},
      log_(std::move(log)),
      targets_(std::move(deps.targets)),
      remote_(std::move(deps.remote)) {
    runner_ = std::make_unique<TargetRunner>(std::move(deps.start), [this](const std::string& target, const TargetStatus& s) {
        broadcast(state_msg(target, s));
    });
}

PlayPlugin::~PlayPlugin() {
    runner_.reset();   // before the clients and the hub its callbacks reach
    std::vector<std::string> viewers;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [id, c] : clients_) {
            if (!c.viewer.empty()) viewers.push_back(c.viewer);
        }
        clients_.clear();
    }
    for (const auto& v : viewers) remote_->close(v);
    remote_.reset();   // its reaper calls back into the clients: stop it while they still exist
}

std::optional<PlayTarget> PlayPlugin::find_target(const std::string& name) const {
    std::vector<std::string> errors;
    for (auto& t : targets_(errors)) {
        if (t.name == name) return t;
    }
    return std::nullopt;
}

JsonValue PlayPlugin::target_json(const PlayTarget& t) const {
    const TargetStatus s = runner_->status(t.name);
    JsonValue buttons = JsonValue::make_array();
    for (const auto& b : t.buttons) buttons.push(jobj({{"label", jstr(b.label)}, {"key", jstr(b.key)}}));
    JsonValue out = jobj({{"name", jstr(t.name)},
                          {"label", jstr(t.label)},
                          {"profile", jstr(t.profile)},
                          {"window", jstr(t.window)},
                          {"fps", jnum(t.fps)},
                          {"can_build", jbool(t.build.has_value())},
                          {"buttons", buttons},
                          {"state", jstr(state_name(s.state))},
                          {"viewers", jnum(static_cast<double>(remote_->viewers(t.name)))}});
    if (!t.preview_mode.empty()) {
        out.set("scenePreview", jobj({{"app", jstr(t.scene_app)}, {"scene", jstr(t.scene_id)},
                                      {"mode", jstr(t.preview_mode)}, {"state", jstr(t.progress_state)}}));
    }
    out.set("exit_code", s.exit_code ? jnum(*s.exit_code) : JsonValue::make_null());
    return out;
}

JsonValue PlayPlugin::snapshot() const {
    std::vector<std::string> errors;
    const auto list = targets_(errors);
    JsonValue out = jobj({{"ok", jbool(!list.empty())},
                          {"targets", jnum(static_cast<double>(list.size()))},
                          {"rtc", jbool(remote_->streaming())},
                          {"input", jbool(remote_->input())},
                          {"turn", jbool(remote_->turn())}});
    if (list.empty()) out.set("err", jstr("no workspace profile lists \"play\" (profiles: <studio home>/profiles, ERGO_PLUGIN_DIR)"));
    JsonValue errs = JsonValue::make_array();
    for (const auto& e : errors) errs.push(jstr(e));
    out.set("errors", errs);
    return out;
}

net::HttpResponse PlayPlugin::list_targets() const {
    std::vector<std::string> errors;
    JsonValue list = JsonValue::make_array();
    for (const auto& t : targets_(errors)) list.push(target_json(t));
    JsonValue errs = JsonValue::make_array();
    for (const auto& e : errors) errs.push(jstr(e));
    return json_response(200, jobj({{"ok", jbool(true)}, {"targets", list}, {"errors", errs}}));
}

net::HttpResponse PlayPlugin::target_action(const net::HttpRequest& req, const std::string& rest) {
    const auto slash = rest.find('/');
    if (slash == std::string::npos) return fail(404, "not found");
    const std::string name   = rest.substr(0, slash);
    const std::string action = rest.substr(slash + 1);
    if (!is_valid_target_name(name)) return fail(404, "unknown target");
    const auto target = find_target(name);
    if (!target) return fail(404, "unknown target: " + name);

    if (action == "logs") {
        if (req.method != "GET") return fail(405, "method not allowed");
        uint64_t next  = 0;
        JsonValue lines = JsonValue::make_array();
        for (const auto& l : runner_->logs(name, parse_since(req), next)) {
            lines.push(jobj({{"n", jnum(static_cast<double>(l.n))},
                             {"stream", jstr(l.is_stderr ? "stderr" : "stdout")},
                             {"text", jstr(l.text)}}));
        }
        return json_response(200, jobj({{"ok", jbool(true)}, {"next", jnum(static_cast<double>(next))}, {"lines", lines}}));
    }
    if (req.method != "POST") return fail(405, "method not allowed");
    std::string err;
    StartResult r = StartResult::Started;
    if (action == "build") r = runner_->build(*target, err);
    else if (action == "run") r = runner_->run(*target, err);
    else if (action == "stop") {
        if (!runner_->stop(name)) return fail(409, "nothing is running for " + name);
        return json_response(200, jobj({{"ok", jbool(true)}}));
    } else {
        return fail(404, "not found");
    }
    switch (r) {
        case StartResult::Started: return json_response(200, jobj({{"ok", jbool(true)}, {"target", target_json(*target)}}));
        case StartResult::Busy: return fail(409, err);
        case StartResult::NoBuild: return fail(400, err);
        case StartResult::Failed: return fail(500, err);
    }
    return fail(500, err);
}

net::HttpResponse PlayPlugin::handle(const net::HttpRequest& req, const std::string& sub) {
    if (sub == "/api/health") return json_response(200, snapshot());
    if (sub == "/api/targets") {
        if (req.method != "GET") return fail(405, "method not allowed");
        return list_targets();
    }
    if (sub == "/api/ice") {
        if (req.method != "GET") return fail(405, "method not allowed");
        std::string     warning;
        const JsonValue servers = remote_->ice_servers(&warning);
        if (!warning.empty()) log_("warn", "play: " + warning);
        return json_response(200, jobj({{"ok", jbool(true)}, {"iceServers", servers}}));
    }
    const std::string prefix = "/api/targets/";
    if (sub.rfind(prefix, 0) == 0) return target_action(req, sub.substr(prefix.size()));
    return fail(404, "not found");
}

net::WsHandlers PlayPlugin::on_open(const net::HttpRequest&, std::shared_ptr<net::IWsConnection> conn) {
    const uint64_t id = conn->id();
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_[id] = Client{conn, {}, {}, {}};
    }
    net::WsHandlers h;
    h.on_text  = [this, id](const std::string& text) { on_text(id, text); };
    h.on_close = [this, id] { close_client(id); };
    return h;
}

void PlayPlugin::on_text(uint64_t client, const std::string& text) {
    const PlayMessage msg = parse_play_message(text);
    if (const auto* bad = std::get_if<BadMessage>(&msg)) return send(client, error_msg("bad_message", bad->error));
    if (const auto* offer = std::get_if<OfferMessage>(&msg)) return on_offer(client, offer->target, offer->sdp);

    std::string window;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        const auto it = clients_.find(client);
        if (it == clients_.end() || it->second.viewer.empty()) return;   // PLAY-INV-02: viewers only
        window = it->second.window;
    }
    if (!remote_->input()) return send(client, error_msg("input_unsupported"));
    std::string err;   // a missing window drops the input (spec: no error back)
    if (const auto* k = std::get_if<KeyMessage>(&msg)) {
        if (!remote_->known_key(k->name)) return send(client, error_msg("bad_message", "unknown key"));
        remote_->key(window, k->name, k->down, err);
    } else if (const auto* p = std::get_if<PointerMessage>(&msg)) {
        remote_->pointer(window, p->x, p->y, p->button, p->phase, err);
    } else if (const auto* w = std::get_if<WheelMessage>(&msg)) {
        remote_->wheel(window, w->x, w->y, w->notches, err);
    }
}

void PlayPlugin::on_offer(uint64_t client, const std::string& target_name, const std::string& sdp) {
    const auto target = find_target(target_name);
    if (!target) return send(client, error_msg("unknown_target", target_name));
    std::string previous;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        const auto it = clients_.find(client);
        if (it == clients_.end()) return;
        previous = it->second.viewer;
        it->second.viewer.clear();
    }
    if (!previous.empty()) remote_->close(previous);   // one view per connection

    const std::string viewer = "ws-" + std::to_string(client);
    std::string answer, err;
    const bool ok = remote_->open(viewer, *target, sdp, [this, client, name = target->name] {
        send(client, error_msg("stream_ended", "the capture of " + name + " ended"));
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (auto it = clients_.find(client); it != clients_.end()) it->second.viewer.clear();
    }, answer, err);
    if (!ok) return send(client, error_msg(err));
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        const auto it = clients_.find(client);
        if (it == clients_.end()) {   // the socket closed while answering
            remote_->close(viewer);
            return;
        }
        it->second.viewer = viewer;
        it->second.target = target->name;
        it->second.window = target->window;
    }
    send(client, jobj({{"type", jstr("answer")}, {"target", jstr(target->name)}, {"sdp", jstr(answer)}}));
}

void PlayPlugin::close_client(uint64_t client) {
    std::string viewer;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        const auto it = clients_.find(client);
        if (it == clients_.end()) return;
        viewer = it->second.viewer;
        clients_.erase(it);
    }
    if (!viewer.empty()) remote_->close(viewer);
}

void PlayPlugin::send(uint64_t client, const JsonValue& msg) {
    std::shared_ptr<net::IWsConnection> conn;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        const auto it = clients_.find(client);
        if (it == clients_.end()) return;
        conn = it->second.conn;
    }
    if (conn->is_open()) conn->send_text(to_json(msg));
}

void PlayPlugin::broadcast(const JsonValue& msg) {
    std::vector<std::shared_ptr<net::IWsConnection>> conns;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [id, c] : clients_) conns.push_back(c.conn);
    }
    const std::string text = to_json(msg);
    for (const auto& c : conns) {
        if (c->is_open()) c->send_text(text);
    }
}

TargetSource profile_targets(std::shared_ptr<const IProfileSource> profiles) {
    return [profiles = std::move(profiles)](std::vector<std::string>& errors) {
        std::vector<PlayTarget> out;
        const ProfileSet set = profiles->profiles();
        for (const auto& e : set.errors) errors.push_back(e);
        for (const auto& p : set.profiles) {
            if (p.play.is_null()) continue;
            std::string err;
            if (!parse_play_targets(p.play, p.name, p.root, out, err)) errors.push_back(p.source.u8string() + ": " + err);
        }
        return out;
    };
}

std::unique_ptr<host::IPlugin> make_play_plugin(std::shared_ptr<const IProfileSource> profiles, const host::LogFn& log) {
    RemotePlayConfig remote;
    remote.ffmpeg         = process_env("ERGO_STUDIO_FFMPEG").value_or("ffmpeg");
    remote.stun_urls      = process_env("ERGO_STUDIO_STUN_URLS");
    remote.turn_key_id    = process_env("ERGO_STUDIO_TURN_KEY_ID").value_or("");
    remote.turn_key_token = process_env("ERGO_STUDIO_TURN_KEY_API_TOKEN").value_or("");
    if (remote.turn_key_id.empty() != remote.turn_key_token.empty())
        throw std::runtime_error("set both ERGO_STUDIO_TURN_KEY_ID and ERGO_STUDIO_TURN_KEY_API_TOKEN, or neither");
    PlayDeps deps;
    deps.targets = profile_targets(std::move(profiles));
    deps.remote = make_remote_play(remote, log);
    deps.start  = &platform::start_live_process;
    return std::make_unique<PlayPlugin>(std::move(deps), log);
}

} // namespace ergo::studio::plugins::play
