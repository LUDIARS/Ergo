#pragma once

/// Minimal easing primitives used by ergo::world_time::Engine for HitSlow
/// fade in / fade out. Header-only; pure functions of `t in [0, 1]` -> [0, 1].
///
/// Mirrors the subset of DOTween easings actually invoked by the Foundation
/// reference implementation. Add more cases here if a callsite requests one.

namespace ergo::world_time {

enum class Ease {
    Linear,
    InQuad,
    OutQuad,
    InOutQuad,
    InCubic,
    OutCubic,
    InOutCubic,
};

constexpr float clamp01(float t) {
    if (t < 0.0f) return 0.0f;
    if (t > 1.0f) return 1.0f;
    return t;
}

constexpr float ease(float t, Ease e) {
    t = clamp01(t);
    switch (e) {
    case Ease::Linear:
        return t;
    case Ease::InQuad:
        return t * t;
    case Ease::OutQuad: {
        float u = 1.0f - t;
        return 1.0f - u * u;
    }
    case Ease::InOutQuad:
        if (t < 0.5f) return 2.0f * t * t;
        else {
            float u = -2.0f * t + 2.0f;
            return 1.0f - 0.5f * u * u;
        }
    case Ease::InCubic:
        return t * t * t;
    case Ease::OutCubic: {
        float u = 1.0f - t;
        return 1.0f - u * u * u;
    }
    case Ease::InOutCubic:
        if (t < 0.5f) return 4.0f * t * t * t;
        else {
            float u = -2.0f * t + 2.0f;
            return 1.0f - 0.5f * u * u * u;
        }
    }
    return t;   // unreachable
}

constexpr float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace ergo::world_time
