#pragma once
/// lesson_action_feel — how many enemies each wave has and where they appear.
#include "feel_tuning.h"
#include "vec2.h"

#include <vector>

namespace lesson_action_feel {

struct Spawn {
    Vec2 pos;
    int  slot = 0;   ///< index of the spawn point (staggers first attacks)
};

/// Every FeelTuning::boss_every-th wave is the boss alone.
bool is_boss_wave(int wave, const FeelTuning& t);

/// Enemies in wave `wave` (1-based). Fixed mode: FeelTuning::waves, then the boss
/// wave (1), then 0 (= won). Endless: 2, 3, 4, ... capped at endless_max_wave_size,
/// with the boss alone every boss_every-th wave.
int wave_size(int wave, bool endless, const FeelTuning& t);

/// Number of waves to win (the last is the boss); 0 = endless.
int waves_to_win(bool endless, const FeelTuning& t);

/// `count` spawn positions on the arena's edge, the ones farthest from the
/// player first (so nobody appears on top of it). Deterministic.
std::vector<Spawn> spawn_points(int count, Vec2 player, const FeelTuning& t);

} // namespace lesson_action_feel
