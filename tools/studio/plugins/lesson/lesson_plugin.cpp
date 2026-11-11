#include "lesson_plugin.h"

#include "lesson_routes.h"

#include <chrono>
#include <exception>

namespace ergo::studio::plugins::lesson {
namespace {

using Clock = std::chrono::steady_clock;

/// For the log: the kind, status and server code only (never a URL, the token or the link code).
std::string failure_summary(const CallFailure& f) {
    switch (f.kind) {
    case FailureKind::Transport: return "transport error";
    case FailureKind::Timeout: return "timeout";
    case FailureKind::Http: return "HTTP " + std::to_string(f.status) + (f.code.empty() ? "" : " " + f.code);
    case FailureKind::Malformed: return "malformed response: " + f.detail;
    case FailureKind::None: break;
    }
    return "ok";
}

const char* call_name(LessonCall c) {
    switch (c) {
    case LessonCall::Pair: return "pair";
    case LessonCall::State: return "state";
    case LessonCall::Progress: return "progress";
    case LessonCall::None: break;
    }
    return "";
}

const char* demo_status_name(DemoStatus s) {
    switch (s) {
    case DemoStatus::NotCued: return "not cued";
    case DemoStatus::Applied: return "applied";
    case DemoStatus::Requested: return "requested";
    case DemoStatus::NotFollowing: return "requested again: the game is not following";
    case DemoStatus::WaitingForGame: return "waiting for the game to bind the variable";
    case DemoStatus::NotBool: return "the variable is not a bool";
    case DemoStatus::NoVariable: return "no variable for this feature";
    }
    return "";
}

/// One line per demo feature whose wanted value or status changed (names are lesson data, not secrets).
void log_demo_changes(const host::LogFn& log, const std::vector<DemoEntry>& before, const std::vector<DemoEntry>& after) {
    for (const auto& e : after) {
        bool same = false;
        for (const auto& old : before) {
            if (old.feature == e.feature) {
                same = old == e;
                break;
            }
        }
        if (same || !e.on) continue;
        std::string line = "[lesson] demo " + e.feature;
        if (!e.variable.empty()) line += " (" + e.variable + ")";
        line += std::string(" = ") + (*e.on ? "ON" : "OFF") + ": " + demo_status_name(e.status);
        log(e.status == DemoStatus::Applied || e.status == DemoStatus::Requested ? "info" : "warn", line);
    }
}

} // namespace

LessonPlugin::LessonPlugin(std::shared_ptr<platform::IHttpClient> http, std::shared_ptr<IGameVariables> variables,
                           host::LogFn log)
    : info_{"lesson", "授業", "odeum-lesson の授業とつなぎ、課題の進み具合の報告 (生徒) と実演の切り替え (実演) を自動で行う。",
            "\xF0\x9F\x8E\x93"},
      log_(std::move(log)),
      session_(LessonClient(std::move(http)), std::move(variables)),
      worker_([this] { run(); }) {}

LessonPlugin::~LessonPlugin() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        stop_ = true;
    }
    cv_.notify_all();
    worker_.join();
}

std::optional<std::string> LessonPlugin::connect(const std::string& link_text) {
    auto parsed = parse_lesson_link(link_text);
    std::optional<std::string> rejected;
    if (!parsed.link) rejected = parsed.error;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (!parsed.link) {
            shown_.link_error = std::move(parsed.error);
        } else {
            shown_.link_error.clear();
            shown_.connecting = true;
            wanted_link_      = std::move(*parsed.link);
            wants_disconnect_ = false;
        }
        ++revision_;
    }
    cv_.notify_all();
    return rejected;
}

net::HttpResponse LessonPlugin::handle(const net::HttpRequest& req, const std::string& sub_path) {
    return handle_lesson_route(*this, req, sub_path, log_);
}

void LessonPlugin::disconnect() {
    {
        std::lock_guard<std::mutex> lk(mtx_);
        wanted_link_.reset();
        wants_disconnect_ = true;
        shown_.link_error.clear();
        ++revision_;
    }
    cv_.notify_all();
}

LessonShown LessonPlugin::shown() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return shown_;
}

JsonValue LessonPlugin::snapshot() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return jobj({{"ok", jbool(true)},
                 {"linked", jbool(shown_.session.linked)},
                 {"role", jstr(role_name(shown_.session.linked ? shown_.session.role : Role::None))}});
}

void LessonPlugin::publish(bool connecting) {
    SessionView now = session_.view();
    if (now == shown_.session && connecting == shown_.connecting) return;
    log_changes(shown_.session, now);
    shown_.session    = std::move(now);
    shown_.connecting = connecting;
    ++revision_;
}

void LessonPlugin::log_changes(const SessionView& before, const SessionView& after) const {
    if (!log_) return;
    if (after.linked && !before.linked) log_("info", std::string("[lesson] linked (role=") + role_name(after.role) + ")");
    if (!after.linked && before.linked) {
        log_("info", after.end == LinkEnd::Unlinked ? "[lesson] unlinked by the server (tool_unlinked)" : "[lesson] disconnected");
    }
    log_demo_changes(log_, before.demo, after.demo);
    if (after.failure.failed() && (after.failure != before.failure || after.failed_call != before.failed_call)) {
        log_("warn", std::string("[lesson] ") + call_name(after.failed_call) + " failed: " + failure_summary(after.failure));
    }
}

void LessonPlugin::run() noexcept {
    // Thread boundary: nothing may escape. A failure here is a bug; it is shown and logged, and the worker stops.
    std::string what;
    try {
        work();
        return;
    } catch (const std::exception& e) {
        what = e.what();
    } catch (...) {
        what = "unknown exception";
    }
    if (log_) {
        try {
            log_("error", "[lesson] worker stopped: " + what);
        } catch (...) {   // NOLINT(bugprone-empty-catch): the log sink itself failed; there is nowhere left to report to
        }
    }
    std::lock_guard<std::mutex> lk(mtx_);
    shown_.connecting = false;
    shown_.link_error = "内部エラーで授業との通信を止めました。Ergo Studio を起動し直してください";
    ++revision_;
}

void LessonPlugin::work() {
    std::unique_lock<std::mutex> lk(mtx_);
    Clock::time_point next_poll = Clock::now();
    while (!stop_) {
        if (wants_disconnect_) {
            wants_disconnect_ = false;
            lk.unlock();
            session_.disconnect();
            lk.lock();
            publish(wanted_link_.has_value());
            continue;
        }
        if (wanted_link_) {
            const LessonLink link = std::move(*wanted_link_);
            wanted_link_.reset();
            lk.unlock();
            session_.link(link);
            session_.poll();   // fill the tab right away (a no-op when pairing failed)
            next_poll = Clock::now() + std::chrono::milliseconds(session_.poll_ms());
            lk.lock();
            publish(wanted_link_.has_value());
            continue;
        }
        if (!session_.linked()) {
            cv_.wait(lk, [this] { return has_command(); });
            continue;
        }
        if (cv_.wait_until(lk, next_poll, [this] { return has_command(); })) continue;
        lk.unlock();
        session_.poll();
        next_poll = Clock::now() + std::chrono::milliseconds(session_.poll_ms());
        lk.lock();
        publish(wanted_link_.has_value());
    }
}

std::unique_ptr<LessonPlugin> make_lesson_plugin(std::shared_ptr<IGameVariables> variables, const host::LogFn& log) {
    return std::make_unique<LessonPlugin>(platform::make_system_http_client(), std::move(variables), log);
}

} // namespace ergo::studio::plugins::lesson
