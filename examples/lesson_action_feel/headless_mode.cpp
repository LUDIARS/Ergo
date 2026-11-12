// --auto (the default): the scripted player (core/autopilot) fights endless
// waves in real time at 60 Hz while every variable is published to the Studio.
// Every switch change and the link state are logged on stdout, plus status
// lines every --status-every seconds. A lost game is logged and restarted.
#include "modes.h"

#include "action_game.h"
#include "autopilot.h"
#include "frame_pacer.h"
#include "lesson_bindings.h"
#include "settings_watch.h"

#include "ergo/bind/bind.h"
#include "ergo/world_time/world_time.h"

#include <atomic>
#include <csignal>
#include <cstdarg>
#include <cstdio>

namespace lesson_action_feel {

namespace {

std::atomic<int> g_stop{0};
void on_signal(int) { g_stop = 1; }

#if defined(__GNUC__)
#define LESSON_PRINTF_LIKE __attribute__((format(printf, 1, 2)))
#else
#define LESSON_PRINTF_LIKE
#endif

void say(const char* fmt, ...) LESSON_PRINTF_LIKE;
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

void report_status(const ActionGame& g, const FeelToggles& t, const CombatSettings& s) {
    say("real=%.1fs world=%.1fs hits=%lld defeated=%lld wave=%d hp=%d hit_stop=%s knockback=%s camera_shake=%s "
        "super_armor=%s hit_stop_last_seconds=%.3f knockback_last_impulse=%.3f knockback_travel=%.2f shake=%.3f",
        g.real_seconds(), g.world_seconds(), static_cast<long long>(g.player_hits()), static_cast<long long>(g.defeated()), g.wave(),
        static_cast<int>(g.player().health.hp()), on_off(t.hit_stop), on_off(t.knockback), on_off(t.camera_shake),
        super_armor_name(s.enemy.super_armor), g.observed().hit_stop_last_seconds,
        g.observed().knockback_last_impulse, g.knockback_travel(), static_cast<double>(g.shake()));
    const SideCounters& e = g.combat().enemy;
    const SideCounters& p = g.combat().player;
    say("combat enemy: flinches=%lld interrupted=%lld armor_absorbed=%lld cancels=%lld | "
        "player: flinches=%lld interrupted=%lld armor_absorbed=%lld cancels=%lld",
        static_cast<long long>(e.flinches), static_cast<long long>(e.interrupted),
        static_cast<long long>(e.armor_absorbed), static_cast<long long>(e.cancels),
        static_cast<long long>(p.flinches), static_cast<long long>(p.interrupted),
        static_cast<long long>(p.armor_absorbed), static_cast<long long>(p.cancels));
}

} // namespace

int run_headless(const Options& o) {
    auto&          world    = ergo::world_time::Engine::instance();
    auto&          bind     = ergo::bind::Engine::instance();
    FeelToggles    toggles  = initial_toggles(o);
    CombatSettings settings = o.combat;
    GameRules      rules;
    rules.endless = true;
    ActionGame     game(initial_tuning(o), rules);
    LessonBindings bindings(toggles, settings, game);
    bindings.bind_all(bind);
    bind.set_app_name("lesson_action_feel");
    if (o.studio) bind.connect(o.host, o.port);
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    say("started (%s): %s", o.plain ? "--plain" : "full", describe_settings(toggles, settings).c_str());
    if (o.studio) say("Studio ws://%s:%u/variable/ws", o.host.c_str(), static_cast<unsigned>(o.port));
    else say("not connecting to a Studio (--no-studio)");

    FramePacer    pacer;
    SettingsWatch watch(toggles, settings);
    double        next_status = o.status_every;
    int           seen_link   = -1;
    while (g_stop == 0) {
        if (o.seconds > 0 && pacer.elapsed() >= o.seconds) break;

        bind.apply_pending_writes();   // sets from the Studio land here, at the start of the frame
        for (const std::string& r : bindings.take_rejections()) say("%s", r.c_str());
        for (const std::string& c : watch.changes(toggles, settings)) say("%s", c.c_str());
        if (o.studio && static_cast<int>(bind.is_connected()) != seen_link) {
            seen_link = static_cast<int>(bind.is_connected());
            say(seen_link ? "connected to the Studio" : "waiting for the Studio (variable plugin)");
        }

        const ActionGame::Frame f = game.frame(pacer.step_seconds(), autopilot(game.view(settings)), toggles, settings, world);
        if (f.wave_started) say("wave %d: %zu enemies", game.wave(), game.enemy_count());
        if (game.status() == GameStatus::Lost) {
            say("game over at wave %d (defeated %lld) -> restart", game.wave(), static_cast<long long>(game.defeated()));
            game.restart();
        }
        if (pacer.elapsed() >= next_status) {
            report_status(game, toggles, settings);
            next_status += o.status_every;
        }
        pacer.wait_next();
    }
    bind.apply_pending_writes();   // publish the last values before leaving
    bind.disconnect();
    report_status(game, toggles, settings);
    say("stopped");
    return 0;
}

} // namespace lesson_action_feel
