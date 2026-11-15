// Windows composition: existing simulation + native window + existing Studio bindings.
#include "modes.h"
#include "options.h"
#include "autopilot.h"
#include "frame_pacer.h"
#include "experiment_controls.h"
#include "lesson_bindings.h"
#include "terminal/key_bindings.h"
#include "windows/arena_window.h"
#include <cstdio>

namespace lesson_action_feel {
namespace {
struct Disconnect {
    ergo::bind::Engine& engine;
    ~Disconnect() { engine.apply_pending_writes(); engine.disconnect(); }
};
void toggle_keys(graphical::ArenaWindow& window, FeelToggles& toggles, CombatSettings& settings) {
    using terminal::Command;
    constexpr Command commands[] = {Command::ToggleAllEffects, Command::ToggleHitStop,
        Command::ToggleKnockback, Command::ToggleCameraShake, Command::ToggleEnemyFlinch,
        Command::CycleEnemySuperArmor, Command::ToggleEnemyCancel, Command::TogglePlayerFlinch,
        Command::CyclePlayerSuperArmor, Command::TogglePlayerCancel};
    for (unsigned i = 0; i < 10; ++i)
        if (window.pressed('0' + i)) terminal::apply_setting(commands[i], toggles, settings);
}
} // namespace
int run_play(const Options& o) {
    graphical::ArenaWindow window;
    if (!window.open()) {
        std::fputs("Could not create the action-game window.\n", stderr);
        return 2;
    }
    auto& world = ergo::world_time::Engine::instance();
    auto& bind = ergo::bind::Engine::instance();
    FeelToggles toggles = initial_toggles(o);
    CombatSettings settings = o.combat;
    GameSession session(initial_tuning(o), initial_rules(o, o.endless || o.autopilot));
    LessonBindings bindings(toggles, settings, session);
    bindings.bind_all(bind);
    Disconnect disconnect{bind}; // destroyed before bindings and the referenced state
    bind.set_app_name("lesson_action_feel");
    if (o.studio) bind.connect(o.host, o.port);
    FramePacer pacer;
    ExperimentControls experiments(o);
    bool paused = false, effects = false, vectors = o.debug_vectors;
    while (window.pump()) {
        if (window.pressed(VK_ESCAPE) || window.pressed('Q') || (o.seconds > 0 && pacer.elapsed() >= o.seconds)) break;
        bind.apply_pending_writes();
        toggle_keys(window, toggles, settings);
        if (window.pressed('O')) effects = !effects;
        if (window.pressed('V')) vectors = !vectors;
        if (effects) {
            for (unsigned i = 0; i < kEffectCount; ++i)
                if (window.pressed('A' + i)) terminal::toggle_effect(effect_table()[i].id, toggles);
        }
        if (!window.active()) paused = true;
        if (window.pressed('P') && window.active()) paused = !paused;
        if (window.pressed('R')) experiments.restart(session, toggles, settings);
        constexpr Experiment presets[] = {Experiment::Baseline, Experiment::LongStop,
            Experiment::ExcessiveKnockback, Experiment::ExcessiveShake};
        for (unsigned i = 0; i < 4; ++i)
            if (window.pressed(VK_F1 + i)) experiments.select(presets[i], session, toggles, settings);
        if (window.pressed(VK_F5)) experiments.reload(session, toggles, settings);
        if (window.pressed(VK_F6)) experiments.rollback(session, toggles, settings);
        const bool key_attack = window.pressed('J'), mouse_attack = window.pressed(VK_LBUTTON);
        const bool attack = key_attack || mouse_attack;
        const bool weapon = window.pressed('K');
        if (!paused && !effects) {
            auto cmd = o.autopilot ? autopilot(session.view(settings, toggles)) : PlayerCommand{};
            if (!o.autopilot) {
                cmd.intent.move = Vec2{float(window.held('D') || window.held(VK_RIGHT)) - float(window.held('A') || window.held(VK_LEFT)),
                                       float(window.held('S') || window.held(VK_DOWN)) - float(window.held('W') || window.held(VK_UP))}.normalized();
                cmd.intent.aim = window.aim(session.view(settings, toggles));
                cmd.intent.attack = attack;
            }
            cmd.switch_weapon = weapon;
            try {
                session.frame(pacer.step_seconds(), cmd, toggles, settings, world);
            } catch (const std::exception& error) {
                experiments.reject(session, toggles, settings, error.what());
                paused = true;
            }
        }
        std::wstring message = !o.studio ? L"Local play / Studio disabled" :
                                bind.is_connected() ? L"Studio connected" : L"Studio waiting for connection";
        if (!bindings.take_rejections().empty()) message += L" / Studio value rejected: outside allowed range";
        message += L" / " + experiments.message();
        window.show(session.view(settings, toggles), {toggles, settings, paused, effects, vectors, message});
        pacer.wait_next();
    }
    world.force_stop();
    return 0;
}
} // namespace lesson_action_feel
