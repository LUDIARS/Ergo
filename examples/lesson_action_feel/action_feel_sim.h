#pragma once
/// lesson_action_feel — the simulation behind the sample: a player hits an
/// enemy at a fixed rhythm (world time), and three "feel" effects can be on
/// or off. Not a playable game: no input, no rendering. It exists so the
/// Odeum lesson's sample course has real variables to look at and switch
/// (spec/feature/tool/lesson.md, "サンプル").
///
/// Deterministic: frame() depends only on its arguments and the state built
/// by earlier frames. Hit stop goes through the real ergo_world_time engine
/// that the caller passes in (the frame's world dt comes from it), so the
/// observations report what that engine actually did.
#include "ergo/world_time/world_time.h"

namespace lesson_action_feel {

/// The three toggles the lesson switches (bound as feel.hit_stop / feel.knockback / feel.camera_shake).
struct FeelToggles {
    bool hit_stop     = true;
    bool knockback    = true;
    bool camera_shake = true;
};

/// What the simulation really did, published read-only so lesson checks can see it.
struct FeelObserved {
    double hit_stop_last_seconds  = 0.0;   ///< duration of the last hit stop the world-time engine entered; 0 until one
    double knockback_last_impulse = 0.0;   ///< magnitude of the last knockback impulse applied; 0 until one
};

struct FeelTuning {
    float attack_interval = 0.5f;    ///< world seconds between two hits
    float hit_stop        = 0.08f;   ///< seconds of freeze per hit
    float knockback       = 6.0f;    ///< impulse (units / s) added to the enemy per hit
    float enemy_spring    = 40.0f;   ///< pulls the knocked-back enemy to its spot
    float enemy_damping   = 9.0f;
    float shake           = 0.3f;    ///< camera shake amplitude per hit
    float shake_decay     = 6.0f;    ///< 1 / s
};

class ActionFeelSim {
public:
    explicit ActionFeelSim(FeelTuning tuning = {}) : tuning_(tuning) {}

    struct Frame {
        float world_dt = 0.0f;
        bool  hit      = false;   ///< the player landed a hit this frame
    };

    /// One frame of `real_dt` seconds with the toggles as they are now.
    /// Calls `world.update(real_dt)` exactly once and `world.hit_stop()` on a
    /// hit when hit stop is on.
    Frame frame(float real_dt, const FeelToggles& toggles, ergo::world_time::Engine& world);

    const FeelObserved& observed() const { return observed_; }
    long  hits() const { return hits_; }
    float enemy_offset() const { return enemy_offset_; }   ///< 0 = at its spot, > 0 = pushed back
    float shake() const { return shake_; }
    double world_seconds() const { return world_seconds_; }

private:
    FeelTuning   tuning_;
    FeelObserved observed_;
    float        until_attack_  = 0.0f;
    float        enemy_offset_  = 0.0f;
    float        enemy_speed_   = 0.0f;
    float        shake_         = 0.0f;
    long         hits_          = 0;
    double       world_seconds_ = 0.0;
};

} // namespace lesson_action_feel
