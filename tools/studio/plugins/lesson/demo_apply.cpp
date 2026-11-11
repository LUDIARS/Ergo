#include "demo_apply.h"

namespace ergo::studio::plugins::lesson {
namespace {

/// The game was already asked for the wanted value at an earlier poll.
bool was_requested(DemoStatus s) { return s == DemoStatus::Requested || s == DemoStatus::NotFollowing; }

} // namespace

bool operator==(const DemoEntry& a, const DemoEntry& b) {
    return a.feature == b.feature && a.variable == b.variable && a.on == b.on && a.status == b.status;
}

void DemoApplier::observe(const std::map<std::string, bool>& states, const std::map<std::string, std::string>& features) {
    auto previous_status = [&](const DemoEntry& e) {
        for (const auto& old : entries_) {
            if (old.feature == e.feature && old.variable == e.variable && old.on == e.on) return old.status;
        }
        return DemoStatus::WaitingForGame;   // not looked at yet: apply() decides
    };

    std::vector<DemoEntry> next;
    for (const auto& [feature, variable] : features) {
        DemoEntry e;
        e.feature  = feature;
        e.variable = variable;
        if (const auto it = states.find(feature); it != states.end()) {
            e.on     = it->second;
            e.status = previous_status(e);
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
}

void DemoApplier::apply(IGameVariables& variables) {
    for (auto& e : entries_) {
        if (!e.on || e.status == DemoStatus::NoVariable) continue;
        const auto current = variables.find(e.variable);
        if (!current) {
            e.status = DemoStatus::WaitingForGame;
            continue;
        }
        if (current->kind != "bool") {
            e.status = DemoStatus::NotBool;
            continue;
        }
        if (current->value.is_bool() && current->value.b == *e.on) {
            e.status = DemoStatus::Applied;
            continue;
        }
        if (!variables.request_set(e.variable, jbool(*e.on))) {
            e.status = DemoStatus::WaitingForGame;   // unbound between find() and the request
            continue;
        }
        e.status = was_requested(e.status) ? DemoStatus::NotFollowing : DemoStatus::Requested;
    }
}

void DemoApplier::reset() { entries_.clear(); }

} // namespace ergo::studio::plugins::lesson
