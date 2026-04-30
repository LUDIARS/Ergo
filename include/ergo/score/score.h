#pragma once

/// ergo::score — score counter with optional combo multiplier and high-score
/// callback.
///
/// Plain math + callbacks; no I/O. Hosts decide where the high score is
/// persisted (file / DB / cloud). Pair with `ergo_io` if you want a quick
/// local high-score file.
///
/// Spec: spec/module/score.md
/// Lexicon: spec/game-lexicon/features/core/score-system.toml (Ars)

#include <cstdint>
#include <functional>

namespace ergo::score {

struct Config {
    /// Use combo multiplier on `add()`.
    bool combo_multiplier = true;

    /// Multiplier formula: `1 + combo * combo_factor` (clamped to >= 1).
    float combo_factor = 0.1f;

    /// Optional cap on the multiplier (0 = no cap).
    float multiplier_cap = 0.0f;
};

class Score {
public:
    using HighScoreHandler = std::function<void(std::int64_t new_high)>;
    using ChangeHandler    = std::function<void(std::int64_t score)>;

    Score() = default;
    explicit Score(Config cfg) : cfg_(cfg) {}

    /// Add raw points. The applied amount may be larger if a combo
    /// multiplier is in effect. Returns the points actually added.
    std::int64_t add(std::int64_t base, std::int32_t combo_count = 0) {
        std::int64_t applied = base;
        if (cfg_.combo_multiplier && combo_count > 0) {
            float m = 1.0f + static_cast<float>(combo_count) * cfg_.combo_factor;
            if (cfg_.multiplier_cap > 0.0f && m > cfg_.multiplier_cap) {
                m = cfg_.multiplier_cap;
            }
            if (m < 1.0f) m = 1.0f;
            applied = static_cast<std::int64_t>(static_cast<double>(base) * static_cast<double>(m));
        }
        score_ += applied;
        if (on_change_) on_change_(score_);
        if (score_ > high_score_) {
            high_score_ = score_;
            if (on_high_score_) on_high_score_(high_score_);
        }
        return applied;
    }

    /// Reset the running score (e.g. game over). High score is preserved.
    void reset() {
        score_ = 0;
        if (on_change_) on_change_(score_);
    }

    /// Initialise the high score (e.g. loaded from disk). Does **not** fire
    /// `on_high_score`.
    void set_high_score(std::int64_t v) noexcept { high_score_ = v; }

    [[nodiscard]] std::int64_t score()       const noexcept { return score_; }
    [[nodiscard]] std::int64_t high_score()  const noexcept { return high_score_; }

    void set_on_change(ChangeHandler h) { on_change_ = std::move(h); }
    void set_on_high_score(HighScoreHandler h) { on_high_score_ = std::move(h); }

private:
    Config cfg_{};
    std::int64_t score_ = 0;
    std::int64_t high_score_ = 0;
    ChangeHandler on_change_{};
    HighScoreHandler on_high_score_{};
};

}  // namespace ergo::score
