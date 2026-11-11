/// lesson_action_feel — headless sample for the Odeum lesson link
/// (spec/feature/tool/lesson.md, "サンプル"). NOT a playable game: a
/// deterministic simulation of a player hitting an enemy (action_feel_sim.h)
/// that publishes the variables of the Odeum sample course through ergo_bind
/// to an Ergo Studio's `variable` plugin:
///
///   feel.hit_stop / feel.knockback / feel.camera_shake    bool, writable from the Studio
///   feel.hit_stop_last_seconds                            double, read-only: last hit stop really applied (0 until one)
///   feel.knockback_last_impulse                           double, read-only: last knockback impulse really applied
///
///   ergo_example_lesson_action_feel [--host H] [--port P] [--plain] [--seconds S] [--status-every S]
///       real time (60 Hz) until Ctrl+C / SIGTERM or S seconds; connects to ws://H:P/variable/ws
///       (default 127.0.0.1:5170). --plain starts with all three effects OFF (演出を抜いた状態).
///   ergo_example_lesson_action_feel --check
///       offline: fixed-dt frames without a Studio; verifies the observations follow the toggles.
///   ergo_example_lesson_action_feel --help | -h
///       prints the usage (stdout, exit 0). An unknown option or a bad value is an error (exit 2).
///
/// Every toggle change (from the Studio or not) and the link state are logged
/// on stdout, plus a status line every --status-every seconds (default 2).

#include "action_feel_sim.h"

#include "ergo/bind/bind.h"
#include "ergo/world_time/world_time.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

using lesson_action_feel::ActionFeelSim;
using lesson_action_feel::FeelToggles;

std::atomic<int> g_stop{0};
void on_signal(int) { g_stop = 1; }

struct Options {
    bool          help  = false;
    bool          check = false;
    bool          plain = false;
    std::string   host  = "127.0.0.1";
    std::uint16_t port  = 5170;
    double        seconds      = 0.0;   ///< 0 = until a signal
    double        status_every = 2.0;
};

bool parse_number(const char* s, double lo, double hi, double& out) {
    char*        end = nullptr;
    const double v   = std::strtod(s, &end);
    if (end == s || *end != '\0' || v < lo || v > hi) return false;
    out = v;
    return true;
}

const char* const kUsage =
    "usage: ergo_example_lesson_action_feel [--host H] [--port P] [--plain] [--seconds S] [--status-every S]\n"
    "       ergo_example_lesson_action_feel --check\n"
    "       ergo_example_lesson_action_feel --help | -h\n"
    "\n"
    "Headless sample for the Odeum lesson link (not a playable game): publishes feel.hit_stop,\n"
    "feel.knockback, feel.camera_shake (writable) and feel.hit_stop_last_seconds,\n"
    "feel.knockback_last_impulse (read-only) to the Studio's variable plugin at ws://H:P/variable/ws.\n"
    "\n"
    "  --host H           Studio host (default 127.0.0.1)\n"
    "  --port P           Studio port, 1..65535 (default 5170; the Studio's PORT)\n"
    "  --plain            start with all three effects OFF\n"
    "  --seconds S        stop after S seconds of real time (default 0 = until Ctrl+C / SIGTERM)\n"
    "  --status-every S   status line every S seconds, 0.1..3600 (default 2)\n"
    "  --check            offline self-check without a Studio (exit 0 = the observations follow the toggles)\n"
    "  --help, -h         print this text and exit\n";

/// "" when every argument is understood, else what is wrong with the first one that is not.
std::string parse_options(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a     = argv[i];
        const bool        value = i + 1 < argc;
        double            n     = 0.0;
        if (a == "--help" || a == "-h") {
            o.help = true;
        } else if (a == "--check") {
            o.check = true;
        } else if (a == "--plain") {
            o.plain = true;
        } else if (a == "--host" || a == "--port" || a == "--seconds" || a == "--status-every") {
            if (!value) return "option " + a + " needs a value";
            const char* v  = argv[++i];
            bool        ok = true;
            if (a == "--host") o.host = v;
            else if (a == "--port" && (ok = parse_number(v, 1, 65535, n))) o.port = static_cast<std::uint16_t>(n);
            else if (a == "--seconds" && (ok = parse_number(v, 0, 1e9, n))) o.seconds = n;
            else if (a == "--status-every" && (ok = parse_number(v, 0.1, 3600, n))) o.status_every = n;
            if (!ok) return "bad value for " + a + ": \"" + v + "\"";
        } else {
            return "unknown option \"" + a + "\"";
        }
    }
    return {};
}

void say(const char* fmt, ...) {
    std::printf("[lesson_action_feel] ");
    va_list ap;
    va_start(ap, fmt);
    std::vprintf(fmt, ap);
    va_end(ap);
    std::printf("\n");
    std::fflush(stdout);
}

const char* on_off(bool b) { return b ? "ON" : "OFF"; }

void report_toggle(const char* name, bool before, bool now) {
    if (before != now) say("%s: %s -> %s", name, on_off(before), on_off(now));
}

void report_status(const ActionFeelSim& sim, const FeelToggles& t) {
    say("world=%.1fs hits=%ld hit_stop=%s knockback=%s camera_shake=%s hit_stop_last_seconds=%.3f "
        "knockback_last_impulse=%.3f enemy_offset=%.3f shake=%.3f",
        sim.world_seconds(), sim.hits(), on_off(t.hit_stop), on_off(t.knockback), on_off(t.camera_shake),
        sim.observed().hit_stop_last_seconds, sim.observed().knockback_last_impulse, sim.enemy_offset(), sim.shake());
}

/// --check: the observations come from what the simulation did, so they stay 0 while an effect is off
/// and become > 0 after the next hit once it is turned on.
int run_check() {
    auto&         world = ergo::world_time::Engine::instance();
    ActionFeelSim sim;
    FeelToggles   toggles{false, false, false};
    int           failures = 0;
    auto expect = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            say("check failed: %s", what);
        }
    };
    for (int i = 0; i < 120; ++i) sim.frame(1.0f / 60.0f, toggles, world);
    expect(sim.hits() >= 3, "the player keeps hitting");
    expect(sim.observed().hit_stop_last_seconds == 0.0, "no hit stop while feel.hit_stop is OFF");
    expect(sim.observed().knockback_last_impulse == 0.0, "no knockback while feel.knockback is OFF");
    toggles.hit_stop  = true;
    toggles.knockback = true;
    for (int i = 0; i < 60; ++i) sim.frame(1.0f / 60.0f, toggles, world);
    expect(sim.observed().hit_stop_last_seconds > 0.0, "a hit stop after feel.hit_stop turned ON");
    expect(sim.observed().knockback_last_impulse > 0.0, "a knockback after feel.knockback turned ON");
    report_status(sim, toggles);
    say("check: %s", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    if (const std::string err = parse_options(argc, argv, opt); !err.empty()) {
        std::fprintf(stderr, "ergo_example_lesson_action_feel: %s (--help lists the options)\n", err.c_str());
        return 2;
    }
    if (opt.help) {
        std::fputs(kUsage, stdout);
        return 0;
    }
    if (opt.check) return run_check();

    auto&         world = ergo::world_time::Engine::instance();
    auto&         bind  = ergo::bind::Engine::instance();
    ActionFeelSim sim;
    FeelToggles   toggles;
    if (opt.plain) toggles = FeelToggles{false, false, false};

    ergo::bind::VarMeta feel;
    feel.category = "feel";
    ergo::bind::VarMeta observed = feel;
    observed.read_only           = true;
    ergo::bind::VarMeta seconds  = observed;
    seconds.unit                 = "s";
    bind.bind("feel.hit_stop", &toggles.hit_stop, feel);
    bind.bind("feel.knockback", &toggles.knockback, feel);
    bind.bind("feel.camera_shake", &toggles.camera_shake, feel);
    bind.bind_accessor(
        "feel.hit_stop_last_seconds", ergo::bind::VarKind::Double,
        [&sim] { return ergo::bind::Value::of_double(sim.observed().hit_stop_last_seconds); }, {}, seconds);
    bind.bind_accessor(
        "feel.knockback_last_impulse", ergo::bind::VarKind::Double,
        [&sim] { return ergo::bind::Value::of_double(sim.observed().knockback_last_impulse); }, {}, observed);
    bind.set_app_name("lesson_action_feel");
    bind.connect(opt.host, opt.port);
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    say("started (%s): hit_stop=%s knockback=%s camera_shake=%s, Studio ws://%s:%u/variable/ws", opt.plain ? "--plain" : "full",
        on_off(toggles.hit_stop), on_off(toggles.knockback), on_off(toggles.camera_shake), opt.host.c_str(),
        static_cast<unsigned>(opt.port));

    using Clock = std::chrono::steady_clock;
    const auto  start       = Clock::now();
    auto        next_tick   = start;
    auto        next_status = start + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(opt.status_every));
    FeelToggles seen        = toggles;
    int         seen_link   = -1;
    while (g_stop == 0) {
        const auto now = Clock::now();
        if (opt.seconds > 0 && now - start >= std::chrono::duration<double>(opt.seconds)) break;

        bind.apply_pending_writes();   // sets from the Studio land here, at the start of the frame
        report_toggle("feel.hit_stop", seen.hit_stop, toggles.hit_stop);
        report_toggle("feel.knockback", seen.knockback, toggles.knockback);
        report_toggle("feel.camera_shake", seen.camera_shake, toggles.camera_shake);
        seen = toggles;
        if (static_cast<int>(bind.is_connected()) != seen_link) {
            seen_link = static_cast<int>(bind.is_connected());
            say(seen_link ? "connected to the Studio" : "waiting for the Studio (variable plugin)");
        }

        sim.frame(1.0f / 60.0f, toggles, world);
        if (now >= next_status) {
            report_status(sim, toggles);
            next_status += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(opt.status_every));
        }

        next_tick += std::chrono::microseconds(16667);
        if (next_tick < Clock::now()) next_tick = Clock::now();   // after a stall: do not burst
        else std::this_thread::sleep_until(next_tick);
    }
    bind.apply_pending_writes();   // publish the last values before leaving
    bind.disconnect();
    report_status(sim, toggles);
    say("stopped");
    return 0;
}
