// --play: the terminal game. Reads keys without blocking, steps the game at a
// fixed 60 Hz, redraws the whole screen every frame, and stays linked to the
// Studio so the lesson's tools can flip the same switches while you play.
#include "modes.h"

#include "action_game.h"
#include "autopilot.h"
#include "frame_pacer.h"
#include "lesson_bindings.h"
#include "settings_watch.h"
#include "terminal/ansi_frame.h"
#include "terminal/key_bindings.h"
#include "terminal/key_decoder.h"
#include "terminal/move_latch.h"
#include "terminal/raw_terminal.h"
#include "terminal/screen.h"

#include "ergo/bind/bind.h"
#include "ergo/world_time/world_time.h"

#include <cstdio>
#include <string>

namespace lesson_action_feel {

namespace {

using namespace lesson_action_feel::terminal;

struct PlayState {
    bool quit          = false;
    bool paused        = false;
    bool show_help     = true;
    bool switch_weapon = false;
    bool restart       = false;
};

void handle(Command c, PlayState& st, MoveLatch& latch, FeelToggles& toggles, CombatSettings& settings) {
    switch (c) {
    case Command::Quit: st.quit = true; return;
    case Command::Pause: st.paused = !st.paused; return;
    case Command::Help: st.show_help = !st.show_help; return;
    case Command::Restart: st.restart = true; return;
    case Command::SwitchWeapon: st.switch_weapon = true; return;
    case Command::None: return;
    default: break;
    }
    if (apply_setting(c, toggles, settings) == nullptr) latch.press(c);   // a move or the attack
}

std::string status_message(const ActionGame& g) {
    switch (g.status()) {
    case GameStatus::Won: return "all waves cleared";
    case GameStatus::Lost: return "you were defeated";
    case GameStatus::Playing: break;
    }
    return {};
}

} // namespace

int run_play(const Options& o) {
    RawTerminal term;
    if (const std::string err = term.open(); !err.empty()) {
        std::fprintf(stderr, "ergo_example_lesson_action_feel: %s\n", err.c_str());
        return 2;
    }
    auto&          world    = ergo::world_time::Engine::instance();
    auto&          bind     = ergo::bind::Engine::instance();
    FeelToggles    toggles  = initial_toggles(o);
    CombatSettings settings = o.combat;
    GameRules      rules;
    rules.endless = o.endless || o.autopilot;
    ActionGame     game(initial_tuning(o), rules);
    LessonBindings bindings(toggles, settings, game);
    bindings.bind_all(bind);
    bind.set_app_name("lesson_action_feel");
    if (o.studio) bind.connect(o.host, o.port);

    KeyDecoder    keys;
    MoveLatch     latch;
    FramePacer    pacer;
    SettingsWatch watch(toggles, settings);
    PlayState     st;
    HudState      hud;
    hud.message = o.autopilot ? "autopilot: the scripted player plays; flip the switches with 1-9"
                              : "defeat every wave; keys below (H hides them)";
    GameStatus last_status = GameStatus::Playing;
    bool       write_ok    = term.write_all(enter_sequence());
    while (write_ok && !st.quit && !RawTerminal::stop_requested()) {
        if (o.seconds > 0 && pacer.elapsed() >= o.seconds) break;
        st.switch_weapon = false;
        st.restart       = false;
        const std::string typed = term.read_available();
        if (typed.empty()) keys.flush();   // a lone ESC with nothing after it for a frame is just ESC
        for (const Key& k : keys.feed(typed)) handle(command_for(k), st, latch, toggles, settings);

        bind.apply_pending_writes();
        for (const std::string& r : bindings.take_rejections()) hud.message = r;
        for (const std::string& c : watch.changes(toggles, settings)) hud.message = c;
        hud.link = !o.studio ? "no Studio" : (bind.is_connected() ? "Studio linked" : "Studio: waiting");

        if (st.restart) {
            game.restart();
            hud.message = "restarted";
        }
        const Intent intent = latch.take(pacer.step_seconds());
        if (!st.paused) {
            PlayerCommand cmd = o.autopilot ? autopilot(game.view(settings)) : PlayerCommand{};
            if (!o.autopilot) cmd.intent = intent;
            cmd.switch_weapon = st.switch_weapon;
            const ActionGame::Frame f = game.frame(pacer.step_seconds(), cmd, toggles, settings, world);
            if (f.wave_started) hud.message = "wave " + std::to_string(game.wave()) + ": " +
                                              std::to_string(game.enemy_count()) + " enemies";
        }
        if (o.autopilot && game.status() == GameStatus::Lost) {
            game.restart();
            hud.message = "autopilot lost; restarted";
        }
        if (game.status() != last_status) {
            last_status = game.status();
            if (last_status != GameStatus::Playing) hud.message = status_message(game);
        }
        hud.toggles   = toggles;
        hud.settings  = settings;
        hud.observed  = game.combat();
        hud.paused    = st.paused;
        hud.show_help = st.show_help;
        write_ok      = term.write_all(compose_frame(render_screen(game.view(settings), hud)));
        pacer.wait_next();
    }
    if (write_ok) term.write_all(leave_sequence());
    term.close();
    bind.apply_pending_writes();
    bind.disconnect();
    std::printf("[lesson_action_feel] %s: wave %d, defeated %lld, hits %lld, score %lld, hp %d\n",
                write_ok ? "quit" : "terminal lost", game.wave(), static_cast<long long>(game.defeated()),
                static_cast<long long>(game.player_hits()), static_cast<long long>(game.score()),
                static_cast<int>(game.player().health.hp()));
    return write_ok ? 0 : 1;
}

} // namespace lesson_action_feel
