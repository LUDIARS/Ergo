#pragma once

/// ergo::combo_counter — successive-success counter that resets on a "break"
/// signal. Used by rhythm games (note hits) and action games (chained
/// combo).
///
/// Spec: spec/module/combo_counter.md
/// Lexicon: spec/game-lexicon/features/rhythm/combo-counter.toml,
///          spec/game-lexicon/features/action/combo-system.toml (Ars)

#include <cstdint>
#include <functional>

namespace ergo::combo_counter {

struct Config {
    /// 0 = no full-combo notion; otherwise, the count required to fire
    /// `on_full_combo`. (Set this to the chart length for rhythm games.)
    std::int32_t full_combo_threshold = 0;
};

class ComboCounter {
public:
    using ChangeHandler = std::function<void(std::int32_t count)>;
    using BreakHandler  = std::function<void(std::int32_t broken_count)>;
    using FullHandler   = std::function<void(std::int32_t count)>;

    ComboCounter() = default;
    explicit ComboCounter(Config cfg) : cfg_(cfg) {}

    /// Successful action — increments the counter.
    void hit() {
        ++count_;
        if (count_ > peak_) peak_ = count_;
        if (on_change_) on_change_(count_);
        if (cfg_.full_combo_threshold > 0
            && count_ == cfg_.full_combo_threshold
            && on_full_combo_) {
            on_full_combo_(count_);
        }
    }

    /// Failed action — resets to 0 and fires `on_break` if there was anything
    /// to break. No-op (no `on_change`) when count is already 0, so callers
    /// can spam `break_()` cheaply on every miss.
    void break_() {
        if (count_ == 0) return;
        if (on_break_) on_break_(count_);
        count_ = 0;
        if (on_change_) on_change_(count_);
    }

    /// Hard reset (e.g. start of song / level). Does **not** fire
    /// `on_break`, fires `on_change(0)` only if needed.
    void reset() {
        if (count_ != 0) {
            count_ = 0;
            if (on_change_) on_change_(count_);
        }
        peak_ = 0;
    }

    [[nodiscard]] std::int32_t count() const noexcept { return count_; }
    [[nodiscard]] std::int32_t peak() const noexcept { return peak_; }

    void set_on_change(ChangeHandler h) { on_change_ = std::move(h); }
    void set_on_break(BreakHandler h) { on_break_ = std::move(h); }
    void set_on_full_combo(FullHandler h) { on_full_combo_ = std::move(h); }

private:
    Config cfg_{};
    std::int32_t count_ = 0;
    std::int32_t peak_  = 0;
    ChangeHandler on_change_{};
    BreakHandler  on_break_{};
    FullHandler   on_full_combo_{};
};

}  // namespace ergo::combo_counter
