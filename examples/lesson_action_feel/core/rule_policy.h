#pragma once
#include <cmath>
#include <functional>
#include <stdexcept>

namespace lesson_action_feel {
enum class RuleKind : unsigned { HitStop = 0, Knockback = 1 };
// Optional dependency inversion seam. The owner supplies code and retains its resources.
using RuleFunction = std::function<float(RuleKind, float, float)>;
inline float evaluate_rule(const RuleFunction& rule, RuleKind kind, float power, float baseline) {
    if (!rule) return baseline;
    const float value = rule(kind, power, baseline);
    const float limit = kind == RuleKind::HitStop ? 5.0f : 200.0f;
    if (!std::isfinite(value) || value < 0 || value > limit)
        throw std::domain_error("Live rule returned a non-finite or out-of-range value");
    return value;
}
} // namespace lesson_action_feel
