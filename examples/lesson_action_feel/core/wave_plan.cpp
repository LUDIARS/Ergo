#include "wave_plan.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace lesson_action_feel {

int wave_size(int wave, bool endless, const FeelTuning& t) {
    if (wave < 1) return 0;
    if (endless) return std::min(wave + 1, t.endless_max_wave_size);
    const std::size_t i = static_cast<std::size_t>(wave - 1);
    return i < t.waves.size() ? t.waves[i] : 0;
}

int waves_to_win(bool endless, const FeelTuning& t) {
    return endless ? 0 : static_cast<int>(t.waves.size());
}

std::vector<Spawn> spawn_points(int count, Vec2 player, const FeelTuning& t) {
    const float w = t.arena_width;
    const float h = t.arena_height;
    // Corners, then the middle of each edge, then the quarter points of the long edges.
    const std::array<Vec2, 10> points{{
        {1.5f, 1.5f}, {w - 1.5f, 1.5f}, {1.5f, h - 1.5f}, {w - 1.5f, h - 1.5f},
        {w * 0.5f, 1.5f}, {w * 0.5f, h - 1.5f}, {1.5f, h * 0.5f}, {w - 1.5f, h * 0.5f},
        {w * 0.25f, 1.5f}, {w * 0.75f, h - 1.5f},
    }};
    std::vector<Spawn> all;
    for (std::size_t i = 0; i < points.size(); ++i) all.push_back({points[i], static_cast<int>(i)});
    std::stable_sort(all.begin(), all.end(), [player](const Spawn& a, const Spawn& b) {
        return (a.pos - player).length() > (b.pos - player).length();
    });
    const std::size_t n = std::min(all.size(), static_cast<std::size_t>(std::max(0, count)));
    all.resize(n);
    for (std::size_t i = 0; i < n; ++i) all[i].slot = static_cast<int>(i);   // stagger by arrival order
    return all;
}

} // namespace lesson_action_feel
