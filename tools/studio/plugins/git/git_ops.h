#pragma once
/// git for the "git" plugin: what a person does by hand in a game workspace
/// (look at the state and history, switch or make a branch, commit, pull,
/// push), on a repository the workspace profiles list. Arguments go to git as
/// a vector (no shell); a commit message is passed through a file (-F).
#include "../common/json_util.h"
#include "ergo/studio/platform/process_runner.h"

#include <filesystem>
#include <memory>
#include <string>

namespace ergo::studio::plugins::git {

struct GitOutcome {
    bool        ok = false;
    std::string message;   ///< git's own words (stdout, else stderr), trimmed
};

/// `git status --porcelain=v2 --branch` -> {branch, upstream, ahead, behind, files: [{path, state}]} (pure).
JsonValue parse_status(const std::string& porcelain_v2);

/// True for a branch name git accepts and the Studio passes on (no leading '-', no spaces, no "..").
bool valid_branch(const std::string& name);

class GitOps {
public:
    GitOps(std::shared_ptr<platform::IProcessRunner> runner, int timeout_ms) : runner_(std::move(runner)), timeout_ms_(timeout_ms) {}

    std::optional<JsonValue> status(const std::filesystem::path& repo, std::string& err) const;
    /// The last `count` commits: [{sha, subject, author, date}].
    std::optional<JsonValue> log(const std::filesystem::path& repo, int count, std::string& err) const;
    /// Local branches: [{name, current}].
    std::optional<JsonValue> branches(const std::filesystem::path& repo, std::string& err) const;

    GitOutcome fetch(const std::filesystem::path& repo) const;
    GitOutcome pull(const std::filesystem::path& repo) const;                     ///< --ff-only
    GitOutcome switch_to(const std::filesystem::path& repo, const std::string& branch, bool create) const;
    /// Stages every change (git add -A) and commits with `message`.
    GitOutcome commit_all(const std::filesystem::path& repo, const std::string& message) const;
    /// Pushes the current branch to origin, setting the upstream.
    GitOutcome push(const std::filesystem::path& repo) const;

private:
    platform::ProcessResult git(const std::filesystem::path& repo, std::vector<std::string> args, int timeout_ms) const;
    GitOutcome              outcome(const platform::ProcessResult& r, const std::string& what) const;

    std::shared_ptr<platform::IProcessRunner> runner_;
    int                                       timeout_ms_;
};

} // namespace ergo::studio::plugins::git
