#include "sonus_plugin.h"

#include "recording_path.h"
#include "sonus_routes.h"

#include "../common/json_util.h"

#include <chrono>
#include <ctime>
#include <exception>
#include <filesystem>
#include <system_error>

namespace ergo::studio::plugins::sonus {
namespace {

/// Messages appended since `before` (the session keeps the newest 64).
std::vector<std::string> new_messages(const std::vector<std::string>& before, const std::vector<std::string>& after) {
    if (before.empty()) return after;
    for (std::size_t i = after.size(); i-- > 0;) {
        if (after[i] == before.back()) return {after.begin() + static_cast<std::ptrdiff_t>(i) + 1, after.end()};
    }
    return after;
}

/// UTF-8 text of a path in C++17 (std::string) and C++20 (std::u8string) alike.
std::string utf8(const std::filesystem::path& p) {
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

} // namespace

SonusPlugin::SonusPlugin(SonusConfig config, host::LogFn log)
    : info_{"sonus", "サウンド",
            "動いているゲームの Sonus にライブリンクでつなぎ、ミキサー・プロファイラ・上書き・録画を扱う。",
            "\xF0\x9F\x94\x8A",
            "connects to the game's Sonus live link (ERGO_SONUS_HOST / ERGO_SONUS_PORT); "
            "without the window: POST http://{addr}/sonus/api/connect, state at GET http://{addr}/sonus/api/state"},
      config_(std::move(config)),
      log_(std::move(log)) {
    if (!config_.project_dir.empty()) {
        project_ok_ = link_.load_project(config_.project_dir, config_.build_dir);
        if (log_) {
            log_(project_ok_ ? "info" : "error",
                 std::string("[sonus] project ") + config_.project_dir + (project_ok_ ? " loaded" : " has errors (see the tab)"));
        }
    }
    {
        std::lock_guard<std::mutex> lk(mtx_);
        shown_ = link_.shown();
    }
    ticker_ = std::thread([this] { run(); });
}

SonusPlugin::~SonusPlugin() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    ticker_.join();
    std::lock_guard<std::mutex> lk(mtx_);
    link_.disconnect();
}

net::HttpResponse SonusPlugin::handle(const net::HttpRequest& req, const std::string& sub_path) {
    return handle_sonus_route(*this, req, sub_path, log_);
}

JsonValue SonusPlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(project_ok_)},
                 {"state", jstr(link_state_name(shown_.state))},
                 {"target", jstr(config_.host + ":" + std::to_string(config_.port))},
                 {"project", jstr(shown_.project_name)}});
}

std::string SonusPlugin::connect() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (link_.is_open()) return "already connected (disconnect first)";
    std::string why = link_.connect(config_.host, config_.port);
    if (log_) {
        const std::string target = config_.host + ":" + std::to_string(config_.port);
        log_(why.empty() ? "info" : "error", why.empty() ? "[sonus] connecting to " + target
                                                         : "[sonus] connect to " + target + " failed: " + why);
    }
    publish();
    return why;
}

void SonusPlugin::disconnect() {
    std::lock_guard<std::mutex> lk(mtx_);
    if (link_.is_open() && log_) log_("info", "[sonus] disconnected");
    link_.disconnect();
    publish();
}

Edit SonusPlugin::set_bus_fader(const std::string& key, float db) {
    std::lock_guard<std::mutex> lk(mtx_);
    const int bus = link_.find_bus(key);
    if (bus < 0) return Edit::NotFound;
    const bool changed = link_.set_bus_fader(bus, db);
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::nudge_bus_fader(const std::string& key, float delta_db) {
    std::lock_guard<std::mutex> lk(mtx_);
    const int bus = link_.find_bus(key);
    if (bus < 0 || static_cast<std::size_t>(bus) >= shown_.buses.size()) return Edit::NotFound;
    // shown_ is current: every edit publishes before it returns.
    const bool changed = link_.set_bus_fader(bus, shown_.buses[static_cast<std::size_t>(bus)].fader_db + delta_db);
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::bus_edit(const std::string& key, bool (SonusLink::*fn)(int, bool), bool on) {
    std::lock_guard<std::mutex> lk(mtx_);
    const int bus = link_.find_bus(key);
    if (bus < 0) return Edit::NotFound;
    const bool changed = (link_.*fn)(bus, on);
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::set_bus_mute(const std::string& key, bool on) { return bus_edit(key, &SonusLink::set_bus_mute, on); }
Edit SonusPlugin::set_bus_solo(const std::string& key, bool on) { return bus_edit(key, &SonusLink::set_bus_solo, on); }

Edit SonusPlugin::undo() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool changed = link_.undo();
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::redo() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool changed = link_.redo();
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::revert(const std::string& key) {
    std::lock_guard<std::mutex> lk(mtx_);
    bool known = false;   // shown_ is current: every edit publishes before it returns
    for (const OverrideShown& o : shown_.overrides) known = known || o.key == key;
    if (!known) return Edit::NotFound;
    const bool changed = link_.revert(key);
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

Edit SonusPlugin::revert_all() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool changed = link_.revert_all();
    publish();
    return changed ? Edit::Changed : Edit::Unchanged;
}

bool SonusPlugin::start_recording() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool ok = link_.start_recording();
    if (log_) log_(ok ? "info" : "warn", ok ? "[sonus] recording started" : "[sonus] recording could not start");
    publish();
    return ok;
}

bool SonusPlugin::stop_recording() {
    std::lock_guard<std::mutex> lk(mtx_);
    const bool ok = link_.stop_recording();
    if (log_) log_(ok ? "info" : "warn", ok ? "[sonus] recording stopping" : "[sonus] no recording to stop");
    publish();
    return ok;
}

std::string SonusPlugin::save_recording(std::string& error) {
    std::lock_guard<std::mutex> lk(mtx_);
    error.clear();
    if (config_.recording_dir.empty()) {
        error = "saving is off: set ERGO_SONUS_RECORDING_DIR to the directory for recordings";
        return {};
    }
    const std::filesystem::path dir = std::filesystem::u8path(config_.recording_dir);
    std::error_code             ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        error = "cannot create the recording directory: " + ec.message();
        return {};
    }
    const std::string path = utf8(dir / recording_file_name(std::time(nullptr), saves_ + 1));
    error                  = link_.save_recording(path);
    if (!error.empty()) {
        if (log_) log_("warn", "[sonus] saving the recording failed: " + error);
        publish();
        return {};
    }
    ++saves_;
    last_saved_ = path;
    if (log_) log_("info", "[sonus] recording saved: " + path);
    publish();
    return path;
}

std::string SonusPlugin::last_saved() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return last_saved_;
}

SonusShown SonusPlugin::shown() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return shown_;
}

void SonusPlugin::publish() {
    SonusShown now = link_.shown();
    if (log_) {
        if (now.state != shown_.state) {
            log_(now.state == LinkState::Rejected ? "warn" : "info",
                 std::string("[sonus] link ") + link_state_name(now.state) + (now.target.empty() ? "" : " (" + now.target + ")"));
        }
        for (const std::string& m : new_messages(shown_.messages, now.messages)) log_("info", "[sonus] " + m);
    }
    shown_ = std::move(now);
    ++revision_;
}

void SonusPlugin::run() noexcept {
    // Thread boundary: nothing may escape. A failure here is a bug; it is logged and the ticker stops.
    std::string what;
    try {
        loop();
        return;
    } catch (const std::exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception";
    }
    if (log_) {
        try {
            log_("error", "[sonus] ticker stopped: " + what);
        } catch (...) {   // NOLINT(bugprone-empty-catch): the log sink itself failed; there is nowhere left to report to
        }
    }
    std::lock_guard<std::mutex> lk(mtx_);
    shown_.messages.push_back("内部エラーでライブリンクの更新を止めました。Ergo Studio を起動し直してください");
    ++revision_;
}

void SonusPlugin::loop() {
    std::unique_lock<std::mutex> lk(mtx_);
    while (!stop_) {
        if (cv_.wait_for(lk, std::chrono::milliseconds(config_.tick_ms), [this] { return stop_; })) break;
        if (!link_.is_open()) continue;
        link_.tick();
        publish();
    }
}

std::unique_ptr<SonusPlugin> make_sonus_plugin(const SonusConfig& config, const host::LogFn& log) {
    return std::make_unique<SonusPlugin>(config, log);
}

} // namespace ergo::studio::plugins::sonus
