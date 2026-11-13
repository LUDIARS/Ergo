#include "ergo/perf/quality_ladder.h"

#include <algorithm>
#include <utility>

namespace ergo::perf {

QualityLadder::QualityLadder(std::vector<StageDecl> stages) : stages_(std::move(stages)) {
    int deepest = 0;
    for (StageDecl& s : stages_) {
        s.lightest = std::max(s.lightest, 0);
        deepest = std::max(deepest, s.lightest);
    }
    for (int level = 1; level <= deepest; ++level) {
        for (std::size_t i = 0; i < stages_.size(); ++i) {
            if (stages_[i].lightest >= level) steps_.push_back({i, level});
        }
    }
}

QualityLadder::QualityLadder(std::vector<StageDecl> stages, std::vector<LadderStep> steps)
    : stages_(std::move(stages)), steps_(std::move(steps)) {}

std::optional<QualityLadder> QualityLadder::from_steps(std::vector<StageDecl> stages,
                                                       std::vector<LadderStep> steps) {
    std::vector<int> level(stages.size(), 0);
    for (const LadderStep& step : steps) {
        if (step.stage >= stages.size()) return std::nullopt;
        if (step.level != level[step.stage] + 1) return std::nullopt;
        if (step.level > stages[step.stage].lightest) return std::nullopt;
        level[step.stage] = step.level;
    }
    return QualityLadder(std::move(stages), std::move(steps));
}

std::optional<std::size_t> QualityLadder::find(std::string_view name) const {
    for (std::size_t i = 0; i < stages_.size(); ++i) {
        if (stages_[i].name == name) return i;
    }
    return std::nullopt;
}

std::vector<int> QualityLadder::levels_at(int step) const {
    std::vector<int> levels(stages_.size(), 0);
    const int last = std::clamp(step, 0, step_count());
    for (int i = 0; i < last; ++i) {
        const LadderStep& s = steps_[static_cast<std::size_t>(i)];
        levels[s.stage] = s.level;
    }
    return levels;
}

} // namespace ergo::perf
