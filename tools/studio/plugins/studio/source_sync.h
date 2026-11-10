#pragma once
/// git status reading and the fast-forward decision (port of
/// studio/domain/source-sync.ts, ST-INV-06). Pure.
#include <optional>
#include <string>

namespace ergo::studio::plugins::studio {

struct BranchStatus {
    std::optional<std::string> branch;     ///< nullopt when detached
    std::optional<std::string> upstream;
    int                        ahead     = 0;
    int                        behind    = 0;
    int                        changed   = 0;   ///< tracked changes (staged / unstaged / conflicts)
    int                        untracked = 0;
};

/// `git status --porcelain=v2 --branch` output.
BranchStatus parse_branch_status(const std::string& text);

struct FastForwardDecision {
    bool        allowed = false;
    std::string branch;   ///< when allowed
    std::string reason;   ///< when not
};

/// Only on the base branch, with an upstream, no changes, no local-only
/// commits, and behind: a shared work tree is never moved with someone
/// else's work in it.
FastForwardDecision decide_fast_forward(const BranchStatus& s, const std::string& base_branch = "main");

} // namespace ergo::studio::plugins::studio
