#pragma once
/// lesson_action_feel — every number of the game in one struct (FeelTuning)
/// so a student can change a value and rebuild, plus what the gameplay
/// effects really did (FeelObserved). The effect switches are the registry in
/// feel_effects.h; the presentation numbers are in PresentationTuning.
///
/// Rules (spec/feature/tool/lesson-action-game.md): a weapon's damage lowers
/// HP; its power sets the knockback impulse (power x knockback_per_power), the
/// hit-stop length (power x hit_stop_per_power, the reference sample's
/// power / 24 s) and the camera shake (power x shake_per_power).
#include "feel_effects.h"

#include <array>
#include <cstdint>

namespace lesson_action_feel {

/// What the two gameplay effects really did, published read-only so lesson checks can see it.
struct FeelObserved {
    double hit_stop_last_seconds  = 0.0;   ///< duration of the last hit stop the world-time engine entered; 0 until one
    double knockback_last_impulse = 0.0;   ///< magnitude of the last knockback impulse applied; 0 until one
};

/// Which way a knockback pushes (a visible design choice, see the spec).
enum class KnockbackDirection : std::uint8_t {
    AwayFromAttacker,   ///< along attacker -> target: wherever you hit from, the target flies away from you (default)
    TargetBackward,     ///< along the target's own backward direction (ActionGameSample's Character.HitBack):
                        ///< a hit from behind pushes the target toward the attacker
};

/// One attack: what it does on a hit and how long its three phases last (world seconds).
struct Weapon {
    const char*  name     = "";
    std::int32_t damage   = 0;      ///< HP removed per hit
    float        power    = 0.0f;   ///< drives knockback impulse, hit-stop length and camera shake
    float        reach    = 0.0f;   ///< from the attacker's centre to the edge of the swing (units)
    float        arc_cos  = 0.0f;   ///< cos of the half-angle of the swing (0 = 180 degree arc, -1 = all around)
    float        windup   = 0.0f;   ///< before the swing can hit (the telegraph)
    float        active   = 0.0f;   ///< the swing hits whatever enters it (each target at most once)
    float        recovery = 0.0f;   ///< after the swing; the side is committed unless it may cancel
    bool         roar     = false;  ///< not a strike: staggers instead of hitting (see combat_rules.h)
    float        stagger  = 0.0f;   ///< roar: flinch seconds forced on whoever it reaches
    bool         charge   = false;  ///< the boss's charge attack (drives the boss blur envelope)
    bool         armored  = false;  ///< super armor during wind-up and swing, whatever the side's setting
};

struct FeelTuning {
    // --- the gameplay effects --------------------------------------------
    float hit_stop_per_power  = 1.0f / 24.0f;   ///< s of freeze per power point (ActionGameSample: power / 24 s)
    float knockback_per_power = 6.0f;           ///< impulse per power point (units / s for mass 1)
    float mass                = 1.0f;           ///< mass of the player and the normal enemies
    float push_friction       = 10.0f;          ///< 1 / s, how fast a knockback speed fades (world time)
    KnockbackDirection knockback_direction = KnockbackDirection::AwayFromAttacker;

    // --- weapons ------------------------------------------------------------
    /// Player weapons (K switches): damage and power both differ.
    std::array<Weapon, 2> player_weapons{{
        {"sword", 20, 2.0f, 1.8f, 0.34f, 0.06f, 0.10f, 0.22f},
        {"hammer", 35, 6.0f, 2.2f, 0.0f, 0.28f, 0.12f, 0.42f},
    }};
    Weapon enemy_weapon{"claw", 12, 3.0f, 1.4f, 0.5f, 0.55f, 0.10f, 0.50f};

    // --- fighters -----------------------------------------------------------
    std::int32_t player_hp     = 100;
    std::int32_t enemy_hp      = 100;
    float        player_speed  = 9.0f;    ///< units / s
    float        enemy_speed   = 3.5f;
    float        radius        = 0.5f;    ///< body radius of the player and the normal enemies
    float        input_buffer  = 0.15f;   ///< an attack pressed too early is kept this long (world s)

    // --- enemies ------------------------------------------------------------
    float enemy_attack_cooldown = 1.8f;   ///< from one attack start to the next (world s)
    float enemy_first_delay     = 0.6f;   ///< before an enemy's first attack ...
    float enemy_delay_step      = 0.45f;  ///< ... plus this per spawn slot, so attacks are staggered
    int   enemy_max_attackers   = 2;      ///< at most this many enemies wind up / swing at once
    int   enemy_chain_max       = 1;      ///< follow-up attacks an enemy chains by motion cancel (when allowed)

    // --- the boss -----------------------------------------------------------
    std::int32_t boss_hp       = 400;
    float        boss_speed    = 2.2f;
    float        boss_radius   = 1.0f;
    float        boss_mass     = 4.0f;    ///< a knockback moves it a quarter as far
    float        boss_min_flinch_power = 4.0f;   ///< hits weaker than this never make the boss flinch (the sword)
    /// Roar: staggers whoever is within 6 units (unless super armor protects them); the roar itself is armored.
    Weapon boss_roar{"roar", 0, 0.0f, 6.0f, -1.0f, 0.50f, 0.60f, 0.60f, true, 0.8f, false, true};
    /// Charge attack: a long telegraphed charge, then a heavy release.
    Weapon boss_charge{"charge", 40, 9.0f, 2.6f, 0.0f, 1.40f, 0.15f, 0.80f, false, 0.0f, true, false};
    float        boss_cooldown = 1.2f;    ///< world s between two boss actions
    std::int64_t boss_points   = 500;

    // --- arena and waves ----------------------------------------------------
    float arena_width   = 30.0f;
    float arena_height  = 12.0f;
    std::array<int, 3> waves{{2, 3, 4}};   ///< enemies per wave; the boss comes after them (wave 4)
    int   boss_every    = 4;               ///< every 4th wave is the boss alone; defeating the first one wins
    int   endless_max_wave_size = 6;
    float intermission  = 1.0f;            ///< world s between a cleared wave and the next
    float corpse_seconds = 0.4f;           ///< a defeated enemy is removed after this (world s; a rule, not a drawing)

    // --- score --------------------------------------------------------------
    std::int64_t defeat_points = 100;
    float        combo_window  = 1.5f;     ///< world s without a player hit before the combo breaks
};

} // namespace lesson_action_feel
