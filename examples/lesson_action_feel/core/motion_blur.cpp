#include "motion_blur.h"

#include <algorithm>
#include <cmath>

namespace lesson_action_feel {

Streak motion_streak(Vec2 motion, const MotionBlurTuning& t) {
    Streak      s;
    const float speed = motion.length();
    const float raw   = speed * t.shutter;
    if (speed <= 0.0f || raw < t.min_length) return s;
    s.length  = std::min(raw, t.max_length);
    s.dir     = motion * (-1.0f / speed);
    s.samples = std::max(1, static_cast<int>(std::ceil(s.length / t.sample_every)));
    return s;
}

} // namespace lesson_action_feel
