#pragma once
/// lesson_action_feel — the arena game's simulation: one player against waves
/// of enemies and the boss, with the per-side combat concepts and the two
/// gameplay effects (hit stop, knockback). Presentation effects live in
/// Presentation (presentation.h) and never change this state.
///
/// Deterministic: frame() depends only on its arguments and the state built by
/// earlier frames. Hit stop goes through the ergo_world_time engine the caller
/// passes in: frame() calls world.update(real_dt) exactly once, and everything
/// here runs on the returned world dt (movement, attack phases, flinch, AI,
/// knockback, wave timers). No I/O, no clock, no rendering.
#include "combat_rules.h"
#include "combat_settings.h"
#include "combatant.h"
#include "enemy_brain.h"
#include "feel_effects.h"
#include "feel_tuning.h"
#include "game_view.h"

#include "ergo/combo_counter/combo_counter.h"
#include "ergo/score/score.h"
#include "ergo/world_time/world_time.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace lesson_action_feel {

struct PlayerCommand {
    Intent intent;
    bool   switch_weapon = false;   ///< next weapon (happens as soon as the player is Ready)
};

struct GameRules {
    bool endless    = false;   ///< waves never end (the headless auto mode uses this)
    int  start_wave = 1;       ///< --boss starts at the boss wave
};

/// One hit (or roar) that landed this frame, for the Presentation.
struct HitEvent {
    int        attacker_id = 0;
    int        target_id   = 0;
    Side       target_side = Side::Enemy;
    Vec2       target_pos;
    HitOutcome outcome;
    bool       roar = false;
};

class ActionGame {
public:
    explicit ActionGame(FeelTuning tuning = {}, GameRules rules = {});

    /// Back to the start wave with full HP. Observations and counters are kept (they are "since start").
    void restart();

    struct Frame {
        float world_dt    = 0.0f;
        int   player_hits = 0;   ///< hits the player landed this frame
        int   enemy_hits  = 0;   ///< hits the player took this frame (roars included)
        int   defeated    = 0;   ///< enemies defeated this frame
        bool  wave_started = false;
        std::vector<HitEvent> hits;
    };

    Frame frame(float real_dt, const PlayerCommand& cmd, const FeelToggles& toggles, const CombatSettings& settings,
                ergo::world_time::Engine& world);

    /// The simulation part of the picture (Presentation adds the effects).
    GameView view(const CombatSettings& settings) const;

    const FeelObserved&   observed() const { return observed_; }
    const CombatObserved& combat() const { return combat_; }
    const Combatant&      player() const { return fighters_.front(); }
    /// Live and dying enemies (index 1.. of the fighters).
    std::size_t      enemy_count() const { return fighters_.size() - 1; }
    const Combatant& enemy(std::size_t i) const { return fighters_[i + 1]; }
    const Combatant* boss() const;          ///< the boss while it is in the arena (alive or dying), else nullptr
    const BrainState* boss_brain() const;
    GameStatus       status() const { return status_; }
    int              wave() const { return wave_; }
    std::int64_t     defeated() const { return defeated_; }
    std::int64_t     player_hits() const { return player_hits_; }
    std::int64_t     score() const { return score_.score(); }
    int              epoch() const { return epoch_; }   ///< +1 at every restart (fighter ids start over)
    double           knockback_travel() const { return knockback_travel_; }
    double           world_seconds() const { return world_seconds_; }
    double           real_seconds() const { return real_seconds_; }
    const FeelTuning& tuning() const { return tuning_; }
    const GameRules&  rules() const { return rules_; }

private:
    void spawn_wave();
    void step_world(float dt, const PlayerCommand& cmd, const FeelToggles& toggles, const CombatSettings& settings,
                    ergo::world_time::Engine& world, Frame& out);
    void move_bodies(float dt);
    void resolve_swings(const FeelToggles& toggles, const CombatSettings& settings, ergo::world_time::Engine& world,
                        Frame& out);
    void land(Combatant& attacker, Combatant& target, const HitOutcome& hit, bool roar,
              ergo::world_time::Engine& world, Frame& out);
    void update_waves(float dt, Frame& out);

    FeelTuning     tuning_;
    GameRules      rules_;
    FeelObserved   observed_;
    CombatObserved combat_;

    std::vector<Combatant>  fighters_;   ///< [0] = player
    std::vector<BrainState> brains_;     ///< parallel to fighters_ (unused for [0])
    std::size_t weapon_index_   = 0;
    bool        switch_pending_ = false;   ///< a weapon switch waits until the player is Ready
    int         next_id_        = 1;
    int         epoch_          = 0;

    ergo::score::Score                score_;
    ergo::combo_counter::ComboCounter combo_;
    float since_player_hit_ = 0.0f;

    GameStatus status_ = GameStatus::Playing;
    int        wave_   = 0;
    float      intermission_left_ = 0.0f;
    bool       between_waves_     = false;
    std::int64_t defeated_    = 0;
    std::int64_t player_hits_ = 0;

    bool   frozen_ = false;
    double knockback_travel_ = 0.0;   ///< distance enemies were moved by knockback, total
    double world_seconds_ = 0.0;
    double real_seconds_  = 0.0;
};

} // namespace lesson_action_feel
