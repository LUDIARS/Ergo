#include "demo_apply.h"

namespace ergo::studio::plugins::lesson {
namespace {

bool is_pending(DemoStatus s) { return s == DemoStatus::WaitingForGame || s == DemoStatus::NotBool; }

} // namespace

bool operator==(const DemoEntry& a, const DemoEntry& b) {
    return a.feature == b.feature && a.variable == b.variable && a.on == b.on && a.status == b.status;
}

void DemoApplier::observe(uint64_t seq, const std::map<std::string, bool>& states,
                          const std::map<std::string, std::string>& features) {
    const bool same_seq = seq_ && *seq_ == seq;
    auto was_applied = [&](const DemoEntry& e) {
        if (!same_seq) return false;
        for (const auto& old : entries_) {
            if (old.status == DemoStatus::Applied && old.feature == e.feature && old.variable == e.variable && old.on == e.on) {
                return true;
            }
        }
        return false;
    };

    std::vector<DemoEntry> next;
    for (const auto& [feature, variable] : features) {
        DemoEntry e;
        e.feature  = feature;
        e.variable = variable;
        if (const auto it = states.find(feature); it != states.end()) {
            e.on     = it->second;
            e.status = was_applied(e) ? DemoStatus::Applied : DemoStatus::WaitingForGame;
        }
        next.push_back(std::move(e));
    }
    for (const auto& [feature, on] : states) {
        if (features.count(feature)) continue;
        DemoEntry e;
        e.feature = feature;
        e.on      = on;
        e.status  = DemoStatus::NoVariable;
        next.push_back(std::move(e));
    }
    entries_ = std::move(next);
    seq_     = seq;
}

void DemoApplier::apply(IGameVariables& variables) {
    for (auto& e : entries_) {
        if (!is_pending(e.status) || !e.on) continue;
        const auto current = variables.find(e.variable);
        if (!current) {
            e.status = DemoStatus::WaitingForGame;
        } else if (current->kind != "bool") {
            e.status = DemoStatus::NotBool;
        } else {
            e.status = variables.request_set(e.variable, jbool(*e.on)) ? DemoStatus::Applied : DemoStatus::WaitingForGame;
        }
    }
}

void DemoApplier::reset() {
    seq_.reset();
    entries_.clear();
}

} // namespace ergo::studio::plugins::lesson
