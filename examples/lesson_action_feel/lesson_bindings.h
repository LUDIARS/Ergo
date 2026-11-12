#pragma once
/// lesson_action_feel — publishes the switches and the observations through
/// ergo_bind to the Studio's variable plugin (both modes use it).
///
/// Writable (the Studio, the lesson's presenter tool, the keys and the CLI all
/// change the same values):
///   feel.<effect>                                           bool, one per entry of the effect registry
///                                                           (feel_effects.h: hit_stop, knockback, camera_shake, ...)
///   feel.all                                                bool: reads true when every effect is on;
///                                                           writing false turns all off, true turns all on
///   feel.super_armor                                        bool, enemy super armor: true = on (a set to true
///                                                           while it is off selects "attack"), false = off
///   combat.<side>.flinch / combat.<side>.motion_cancel      bool   (<side> = player | enemy)
///   combat.<side>.super_armor                               int32  0 off, 1 attack, 2 always
///   combat.<side>.flinch_seconds                            double 0..5
/// Read-only (what really happened):
///   the registry's observations (feel.hit_stop_last_seconds, ...)                double / int64
///   feel.motion_vector_player, feel.motion_vector_last_hit                       vec3 (x, y, 0), units / frame
///   combat.<side>.flinches / .interrupted / .armor_absorbed / .cancels            int64
///   game.wave, game.player_hp, game.boss_hp                                       int32
///   game.defeated, game.player_hits, game.score                                   int64
/// A write outside a variable's range is not applied; it is reported through
/// take_rejections() so the front-end can say so.
#include "session.h"
#include "combat_settings.h"
#include "feel_tuning.h"

#include "ergo/bind/bind.h"

#include <string>
#include <vector>

namespace lesson_action_feel {

class LessonBindings {
public:
    LessonBindings(FeelToggles& toggles, CombatSettings& settings, const GameSession& session);

    void bind_all(ergo::bind::Engine& engine);
    /// Writes refused since the last call (one line each).
    std::vector<std::string> take_rejections();

private:
    void bind_side(ergo::bind::Engine& engine, const std::string& side, SideSettings& s, const SideCounters& n);

    FeelToggles&      toggles_;
    CombatSettings&   settings_;
    const GameSession& session_;
    std::vector<std::string> rejected_;
};

} // namespace lesson_action_feel
