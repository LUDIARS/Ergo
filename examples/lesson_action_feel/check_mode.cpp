// --check: offline, fixed 60 Hz frames, the scripted player, no Studio.
// Verifies that each observation follows its switch: 0 while it is off, > 0
// after it is turned on (the lesson's automatic checks rely on that), and
// that enemy super armor and flinch really change what happens.
#include "modes.h"

#include "action_game.h"
#include "autopilot.h"
#include "settings_watch.h"

#include "ergo/world_time/world_time.h"

#include <cstdio>

namespace lesson_action_feel {

namespace {

constexpr float kDt = 1.0f / 60.0f;

void run_for(ActionGame& g, float seconds, const FeelToggles& t, const CombatSettings& s,
             ergo::world_time::Engine& world) {
    const int frames = static_cast<int>(seconds * 60.0f);
    for (int i = 0; i < frames; ++i) {
        g.frame(kDt, autopilot(g.view(s)), t, s, world);
        if (g.status() == GameStatus::Lost) g.restart();
    }
}

} // namespace

int run_check() {
    auto& world = ergo::world_time::Engine::instance();
    world.force_stop();
    GameRules rules;
    rules.endless = true;
    ActionGame     game({}, rules);
    FeelToggles    toggles{false, false, false};
    CombatSettings settings;
    int            failures = 0;
    auto expect = [&](bool ok, const char* what) {
        if (!ok) {
            ++failures;
            std::printf("[lesson_action_feel] check failed: %s\n", what);
        }
    };
    run_for(game, 4.0f, toggles, settings, world);
    expect(game.player_hits() >= 3, "the scripted player lands hits");
    expect(game.observed().hit_stop_last_seconds == 0.0, "no hit stop while feel.hit_stop is OFF");
    expect(game.observed().knockback_last_impulse == 0.0, "no knockback while feel.knockback is OFF");
    expect(game.knockback_travel() == 0.0, "enemies are not pushed while feel.knockback is OFF");
    expect(game.combat().enemy.flinches > 0, "enemies flinch when hit (combat.enemy.flinch ON)");
    expect(game.combat().enemy.armor_absorbed == 0, "no hit absorbed while enemy super armor is off");

    toggles.hit_stop  = true;
    toggles.knockback = true;
    run_for(game, 3.0f, toggles, settings, world);
    expect(game.observed().hit_stop_last_seconds > 0.0, "a hit stop after feel.hit_stop turned ON");
    expect(game.observed().knockback_last_impulse > 0.0, "a knockback after feel.knockback turned ON");

    settings.enemy.super_armor = SuperArmor::Attack;
    run_for(game, 10.0f, toggles, settings, world);
    expect(game.combat().enemy.armor_absorbed > 0, "hits absorbed after enemy super armor turned on");

    std::printf("[lesson_action_feel] %s; hits=%lld defeated=%lld hit_stop_last_seconds=%.3f "
                "knockback_last_impulse=%.3f enemy flinches=%lld armor_absorbed=%lld player cancels=%lld\n",
                describe_settings(toggles, settings).c_str(), static_cast<long long>(game.player_hits()),
                static_cast<long long>(game.defeated()), game.observed().hit_stop_last_seconds,
                game.observed().knockback_last_impulse, static_cast<long long>(game.combat().enemy.flinches),
                static_cast<long long>(game.combat().enemy.armor_absorbed),
                static_cast<long long>(game.combat().player.cancels));
    std::printf("[lesson_action_feel] check: %s\n", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}

} // namespace lesson_action_feel
