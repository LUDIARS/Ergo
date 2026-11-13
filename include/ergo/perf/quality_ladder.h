#pragma once

/// ergo_perf - the order in which a renderer's stages are lightened.
///
/// Each stage has quality levels 0 (the best) .. `lightest`. The caller sets
/// `lightest` to the lightest level the stage may reach: a stage that must
/// never be dropped declares the lightest level that still draws it ("this
/// stage stays, at level k at most"). The ladder is the run of single steps
/// from all stages at 0 to all stages at their `lightest`; step s applies its
/// first s entries.
///
/// Full spec: spec/feature/module/perf.md

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ergo::perf {

struct StageDecl {
    std::string name;
    int lightest = 1;  ///< the lightest level this stage may reach (>= 0)
};

/// One step of the ladder: `stage` moves to `level`.
struct LadderStep {
    std::size_t stage = 0;
    int level = 0;
};

class QualityLadder {
public:
    /// The default order: every stage one level lighter in declared order, then
    /// again for the next level (first-declared stages give up quality first).
    explicit QualityLadder(std::vector<StageDecl> stages);

    /// A caller-written order. Empty when it is not a ladder: an unknown stage, a
    /// level that is not the stage's next one, or past the stage's `lightest`.
    /// Steps it leaves out keep those stages at their last level.
    [[nodiscard]] static std::optional<QualityLadder> from_steps(std::vector<StageDecl> stages,
                                                                 std::vector<LadderStep> steps);

    /// Steps past full quality (0 .. step_count() are valid steps).
    [[nodiscard]] int step_count() const { return static_cast<int>(steps_.size()); }
    [[nodiscard]] const std::vector<StageDecl>& stages() const { return stages_; }
    [[nodiscard]] const std::vector<LadderStep>& steps() const { return steps_; }
    /// The stage index of `name`, or empty.
    [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const;

    /// Every stage's level at `step` (clamped to 0 .. step_count()).
    [[nodiscard]] std::vector<int> levels_at(int step) const;

private:
    QualityLadder(std::vector<StageDecl> stages, std::vector<LadderStep> steps);

    std::vector<StageDecl> stages_;
    std::vector<LadderStep> steps_;
};

} // namespace ergo::perf
