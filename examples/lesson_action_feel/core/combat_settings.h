#pragma once
/// lesson_action_feel — the per-side combat concepts (flinch, super armor,
/// motion cancel) as settings, plus what really happened per side.
///
/// Definitions (spec/feature/tool/lesson-action-game.md, "戦闘の三つの概念"):
/// - flinch: a hit that is not absorbed interrupts the side's action and
///   freezes it (no move, no attack, no turn) for flinch_seconds;
/// - super armor: while it applies, a hit deals damage only: no flinch, no
///   interrupt, no knockback (hit stop and camera shake still happen);
/// - motion cancel: the recovery of an attack that has landed a hit may be cut
///   short by a new attack or a move.
/// Precedence: death > super armor > flinch; a flinch cannot be cancelled.
#include <cstdint>

namespace lesson_action_feel {

enum class SuperArmor : std::int32_t {
    Off    = 0,
    Attack = 1,   ///< during the wind-up and active phases of the side's own attacks (not the recovery)
    Always = 2,
};

struct SideSettings {
    bool       flinch         = true;
    float      flinch_seconds = 0.35f;   ///< world seconds
    SuperArmor super_armor    = SuperArmor::Off;
    bool       motion_cancel  = false;   ///< cancel the recovery of an attack that hit
};

/// Defaults: a first play where hits interrupt both sides and the player may
/// chain attacks that connect, while enemies commit to every swing.
struct CombatSettings {
    SideSettings player{true, 0.30f, SuperArmor::Off, true};
    SideSettings enemy{true, 0.45f, SuperArmor::Off, false};
};

/// What happened to one side, counted since the start (published read-only).
struct SideCounters {
    std::int64_t flinches       = 0;   ///< hits that put a fighter of this side into flinch
    std::int64_t interrupted    = 0;   ///< of those, hits that broke an attack (wind-up / active / recovery)
    std::int64_t armor_absorbed = 0;   ///< hits taken while super armor applied (damage only)
    std::int64_t cancels        = 0;   ///< attack recoveries this side cut short by motion cancel
};

struct CombatObserved {
    SideCounters player;
    SideCounters enemy;
};

const char* super_armor_name(SuperArmor a);

} // namespace lesson_action_feel
