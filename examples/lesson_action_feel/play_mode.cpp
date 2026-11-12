// --play: the terminal game. Reads keys without blocking, steps the game at a
// fixed 60 Hz, redraws the whole screen every frame, and stays linked to the
// Studio so the lesson's tools can flip the same switches while you play.
#include "modes.h"

#include "autopilot.h"
#include "session.h"
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
    bool quit           = false;
    bool paused         = false;
    bool show_help      = true;
    bool switch_weapon  = false;
    bool restart        = false;
    bool effects_screen = false;
    bool debug_vectors  = false;
};

void handle(const Key& key, PlayState& st, MoveLatch& latch, FeelToggles& toggles, CombatSettings& settings) {
    const Command c = command_for(key);
    if (st.effects_screen) {   // the effects screen takes a..j; O closes it, 0 and Q still work
        if (const auto e = effect_for_screen_key(key)) {
            toggle_effect(*e, toggles);
            return;
        }
        if (c == Command::EffectsScreen) st.effects_screen = false;
        else if (c == Command::Quit) st.quit = true;
        else if (c == Command::ToggleAllEffects) apply_setting(c, toggles, settings);
        return;
    }
    switch (c) {
    case Command::EffectsScreen: st.effects_screen = true; return;
    case Command::DebugVectors: st.debug_vectors = !st.debug_vectors; return;
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
    case GameStatus::Won: return "the boss is down: all waves cleared";
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
    GameSession    session(initial_tuning(o), initial_rules(o, o.endless || o.autopilot));
    const ActionGame& game = session.game();
    LessonBindings bindings(toggles, settings, session);
    bindings.bind_all(bind);
    bind.set_app_name("lesson_action_feel");
    if (o.studio) bind.connect(o.host, o.port);

    KeyDecoder    keys;
    MoveLatch     latch;
    FramePacer    pacer;
    SettingsWatch watch(toggles, settings);
    PlayState     st;
    st.debug_vectors = o.debug_vectors;
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
        for (const Key& k : keys.feed(typed)) handle(k, st, latch, toggles, settings);

        bind.apply_pending_writes();
        for (const std::string& r : bindings.take_rejections()) hud.message = r;
        for (const std::string& c : watch.changes(toggles, settings)) hud.message = c;
        hud.link = !o.studio ? "no Studio" : (bind.is_connected() ? "Studio linked" : "Studio: waiting");

        if (st.restart) {
            session.restart();
            hud.message = "restarted";
        }
        const Intent intent = latch.take(pacer.step_seconds());
        if (!st.paused && !st.effects_screen) {
            PlayerCommand cmd = o.autopilot ? autopilot(session.view(settings, toggles)) : PlayerCommand{};
            if (!o.autopilot) cmd.intent = intent;
            cmd.switch_weapon = st.switch_weapon;
            const ActionGame::Frame f = session.frame(pacer.step_seconds(), cmd, toggles, settings, world);
            if (f.wave_started)
                hud.message = game.boss() ? "wave " + std::to_string(game.wave()) + ": THE BOSS"
                                          : "wave " + std::to_string(game.wave()) + ": " +
                                                std::to_string(game.enemy_count()) + " enemies";
        }
        if (o.autopilot && game.status() == GameStatus::Lost) {
            session.restart();
            hud.message = "autopilot lost; restarted";
        }
        if (game.status() != last_status) {
            last_status = game.status();
            if (last_status != GameStatus::Playing) hud.message = status_message(game);
        }
        hud.toggles   = toggles;
        hud.settings  = settings;
        hud.observed  = game.combat();
        hud.paused         = st.paused;
        hud.show_help      = st.show_help;
        hud.effects_screen = st.effects_screen;
        hud.debug_vectors  = st.debug_vectors;
        hud.last_hit_id    = session.fx().last_hit_target();
        hud.popup_seconds  = session.fx().tuning().popup_seconds;
        write_ok           = term.write_all(compose_frame(render_screen(session.view(settings, toggles), hud)));
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
