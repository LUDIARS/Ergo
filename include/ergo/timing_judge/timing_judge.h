#pragma once

/// ergo::timing_judge — millisecond-window timing judgment for rhythm games.
///
/// Given a "perfect" time and the actual input time, returns one of
/// PERFECT / GREAT / GOOD / MISS based on absolute delta and the
/// configured windows. Pure stateless math + a thin helper that pairs with
/// `ergo::combo_counter`.
///
/// Spec: spec/module/timing_judge.md
/// Lexicon: spec/game-lexicon/features/rhythm/timing-judge.toml (Ars)

#include <cstdint>

namespace ergo::timing_judge {

enum class Judgment : std::uint8_t {
    Perfect = 0,
    Great   = 1,
    Good    = 2,
    Miss    = 3,
};

/// Window half-widths in milliseconds. A judgment fires when the absolute
/// delta is `<= window`. Windows must be monotonically non-decreasing
/// (perfect <= great <= good); the loader rejects otherwise but we do not
/// re-validate at runtime.
struct Windows {
    std::int32_t perfect_ms = 25;
    std::int32_t great_ms   = 60;
    std::int32_t good_ms    = 120;
};

/// Pure: classify a single (target, actual) pair.
/// `delta = actual - target`; positive means late, negative means early.
[[nodiscard]] inline Judgment judge(std::int64_t target_ms,
                                    std::int64_t actual_ms,
                                    const Windows& w) noexcept {
    std::int64_t d = actual_ms - target_ms;
    std::int64_t a = d < 0 ? -d : d;
    if (a <= w.perfect_ms) return Judgment::Perfect;
    if (a <= w.great_ms)   return Judgment::Great;
    if (a <= w.good_ms)    return Judgment::Good;
    return Judgment::Miss;
}

/// Convenience name lookup (English) for logs / UI.
[[nodiscard]] inline const char* name(Judgment j) noexcept {
    switch (j) {
        case Judgment::Perfect: return "PERFECT";
        case Judgment::Great:   return "GREAT";
        case Judgment::Good:    return "GOOD";
        case Judgment::Miss:    return "MISS";
    }
    return "?";
}

/// Should this judgment break the combo, given the player's "minimum kept"
/// judgment? E.g. `breaks_combo(j, Good)` returns true for Miss only;
/// `breaks_combo(j, Great)` returns true for Good and Miss.
[[nodiscard]] inline bool breaks_combo(Judgment j, Judgment min_kept) noexcept {
    return static_cast<std::uint8_t>(j) > static_cast<std::uint8_t>(min_kept);
}

}  // namespace ergo::timing_judge
