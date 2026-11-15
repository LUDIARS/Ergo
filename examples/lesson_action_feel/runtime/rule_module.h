#pragma once
#include "rule_policy.h"
#include <filesystem>
#include <string>

namespace lesson_action_feel {
class RuleModule {
public:
    // Explicit reload only. Missing/invalid modules preserve the current and previous snapshots.
    bool reload(std::string& error);
    bool rollback();
    const RuleFunction& current() const { return current_; }
private:
    RuleFunction current_, previous_;
};
} // namespace lesson_action_feel
