#pragma once
/// lesson_action_feel — the motion-blur trail of one object, from its motion
/// vector (the object's screen-space displacement since the last frame).
/// Pure: no state. The trail points backwards (opposite to the travel), its
/// length is proportional to the displacement, clamped, and absent below the
/// threshold. Front-ends draw it (terminal cells, or a velocity buffer).
#include "presentation_tuning.h"
#include "vec2.h"

namespace lesson_action_feel {

struct Streak {
    Vec2  dir;          ///< unit vector, exactly opposite to the motion (zero when none)
    float length  = 0.0f;
    int   samples = 0;  ///< 0 = no trail
    bool  any() const { return samples > 0; }
};

Streak motion_streak(Vec2 motion, const MotionBlurTuning& t);

} // namespace lesson_action_feel
