#pragma once
/// lesson_action_feel — the three "feel" switches the lesson flips, what they
/// really did, and every number of the game in one struct (FeelTuning) so a
/// student can change a value and rebuild.
///
/// Rules (spec/feature/tool/lesson-action-game.md): a weapon's damage lowers
/// HP; its power sets the knockback impulse (power x knockback_per_power), the
/// hit-stop length (power x hit_stop_per_power, the reference sample's
/// power / 24 s) and the camera shake (power x shake_per_power).
#include <array>
#include <cstdint>

namespace lesson_action_feel {

/// The three toggles the lesson switches (bound as feel.hit_stop / feel.knockback / feel.camera_shake).
/// They act on every hit, whichever side lands it.
struct FeelToggles {
    bool hit_stop     = true;
    bool knockback    = true;
    bool camera_shake = true;
};

/// What the game really did, published read-only so lesson checks can see it.
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
    float        arc_cos  = 0.0f;   ///< cos of the half-angle of the swing (0 = 180 degree arc)
    float        windup   = 0.0f;   ///< before the swing can hit (the telegraph)
    float        active   = 0.0f;   ///< the swing hits whatever enters it (each target at most once)
    float        recovery = 0.0f;   ///< after the swing; the side is committed unless it may cancel
};

struct FeelTuning {
    // --- the feel effects -------------------------------------------------
    float hit_stop_per_power   = 1.0f / 24.0f;   ///< s of freeze per power point (ActionGameSample: power / 24 s)
    float knockback_per_power  = 3.0f;           ///< impulse per power point (units / s for mass 1)
    float mass                 = 1.0f;           ///< every fighter's mass for the impulse
    float push_friction        = 7.0f;           ///< 1 / s, how fast a knockback speed fades (world time)
    float shake_per_power      = 0.15f;          ///< camera shake amplitude (units) per power point
    float shake_max            = 1.0f;
    float shake_decay          = 6.0f;           ///< 1 / s, real time (the camera keeps moving during a hit stop)
    KnockbackDirection knockback_direction = KnockbackDirection::AwayFromAttacker;

    // --- weapons ------------------------------------------------------------
    /// Player weapons (K switches): same idea as the course, damage and power differ.
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
    float        radius        = 0.5f;    ///< every fighter's body radius
    float        input_buffer  = 0.15f;   ///< an attack pressed too early is kept this long (world s)

    // --- enemies ------------------------------------------------------------
    float enemy_attack_cooldown = 1.8f;   ///< from one attack start to the next (world s)
    float enemy_first_delay     = 0.6f;   ///< before an enemy's first attack ...
    float enemy_delay_step      = 0.45f;  ///< ... plus this per spawn slot, so attacks are staggered
    int   enemy_max_attackers   = 2;      ///< at most this many enemies wind up / swing at once
    int   enemy_chain_max       = 1;      ///< follow-up attacks an enemy chains by motion cancel (when allowed)

    // --- arena and waves ----------------------------------------------------
    float arena_width   = 30.0f;
    float arena_height  = 12.0f;
    std::array<int, 3> waves{{2, 3, 4}};   ///< enemies per wave; clearing the last one wins (unless endless)
    int   endless_max_wave_size = 6;
    float intermission  = 1.0f;            ///< world s between a cleared wave and the next
    float corpse_seconds = 0.4f;           ///< a defeated enemy stays visible this long (world s)

    // --- score --------------------------------------------------------------
    std::int64_t defeat_points = 100;
    float        combo_window  = 1.5f;     ///< world s without a player hit before the combo breaks

    // --- presentation hints (real time) ------------------------------------
    float hit_flash_seconds = 0.15f;       ///< the spark at a hit target, visible through the hit stop
};

} // namespace lesson_action_feel
