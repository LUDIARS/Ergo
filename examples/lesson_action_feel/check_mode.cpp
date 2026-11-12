// --check: offline, fixed 60 Hz frames, the scripted player, no Studio.
// Verifies that every effect of the registry stays at "nothing happened" while
// it is off and really happens once it is on (the lesson's automatic checks
// rely on that), that enemy super armor and flinch change what happens, and
// that the boss's roar and charge drive the boss blur.
#include "modes.h"

#include "autopilot.h"
#include "effect_observations.h"
#include "session.h"
#include "settings_watch.h"

#include "ergo/world_time/world_time.h"

#include <cstdio>
#include <string>

namespace lesson_action_feel {

namespace {

constexpr float kDt = 1.0f / 60.0f;

void run_for(GameSession& g, float seconds, const FeelToggles& t, const CombatSettings& s,
             ergo::world_time::Engine& world) {
    const int frames = static_cast<int>(seconds * 60.0f);
    for (int i = 0; i < frames; ++i) {
        g.frame(kDt, autopilot(g.view(s, t)), t, s, world);
        if (g.game().status() == GameStatus::Lost) g.restart();
    }
}

} // namespace

int run_check() {
    auto& world = ergo::world_time::Engine::instance();
    world.force_stop();
    int  failures = 0;
    auto expect   = [&](bool ok, const std::string& what) {
        if (!ok) {
            ++failures;
            std::printf("[lesson_action_feel] check failed: %s\n", what.c_str());
        }
    };

    GameRules rules;
    rules.endless = true;
    GameSession    session({}, rules);
    FeelToggles    toggles = FeelToggles::all(false);
    CombatSettings settings;
    run_for(session, 4.0f, toggles, settings, world);
    const ActionGame& game = session.game();
    expect(game.player_hits() >= 3, "the scripted player lands hits");
    for (const EffectInfo& i : effect_table())
        if (i.id != Effect::BossBlur)   // no boss yet
            expect(effect_observation(i.id, session) == 0.0, std::string(i.observation) + " stays 0 while feel." + std::string(i.name) + " is OFF");
    expect(game.knockback_travel() == 0.0, "enemies are not pushed while feel.knockback is OFF");
    expect(game.combat().enemy.flinches > 0, "enemies flinch when hit (combat.enemy.flinch ON)");
    expect(game.combat().enemy.armor_absorbed == 0, "no hit absorbed while enemy super armor is off");

    toggles.set_all(true);
    run_for(session, 4.0f, toggles, settings, world);
    expect(effect_observation(Effect::HitStop, session) > 0.0, "a hit stop after feel.hit_stop turned ON");
    expect(effect_observation(Effect::Knockback, session) > 0.0, "a knockback after feel.knockback turned ON");
    expect(effect_observation(Effect::CameraShake, session) > 0.0, "a camera shake after feel.camera_shake turned ON");
    expect(effect_observation(Effect::HitSpark, session) > 0.0, "sparks after feel.hit_spark turned ON");
    expect(effect_observation(Effect::DamageNumbers, session) > 0.0, "damage numbers after feel.damage_numbers turned ON");
    expect(effect_observation(Effect::MotionBlur, session) > 0.0, "a motion-blur trail after feel.motion_blur turned ON");

    settings.enemy.super_armor = SuperArmor::Attack;
    run_for(session, 10.0f, toggles, settings, world);
    expect(game.combat().enemy.armor_absorbed > 0, "hits absorbed after enemy super armor turned on");

    // The boss: its roar and charge drive the boss blur (start at the boss wave).
    world.force_stop();
    GameRules boss_rules = rules;
    boss_rules.start_wave = FeelTuning{}.boss_every;
    GameSession    boss_fight({}, boss_rules);
    CombatSettings plain;
    expect(boss_fight.game().boss() != nullptr, "the boss wave has the boss");
    run_for(boss_fight, 12.0f, toggles, plain, world);
    expect(effect_observation(Effect::BossBlur, boss_fight) > 0.0, "a boss blur after the boss roared or charged");

    std::printf("[lesson_action_feel] %s; hits=%lld defeated=%lld enemy flinches=%lld armor_absorbed=%lld "
                "player cancels=%lld boss_blur_last_peak=%.2f\n",
                describe_settings(toggles, settings).c_str(), static_cast<long long>(game.player_hits()),
                static_cast<long long>(game.defeated()), static_cast<long long>(game.combat().enemy.flinches),
                static_cast<long long>(game.combat().enemy.armor_absorbed),
                static_cast<long long>(game.combat().player.cancels), effect_observation(Effect::BossBlur, boss_fight));
    std::printf("[lesson_action_feel] check: %s\n", failures == 0 ? "ok" : "FAILED");
    return failures == 0 ? 0 : 1;
}

} // namespace lesson_action_feel
