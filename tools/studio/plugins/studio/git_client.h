#pragma once
/// git of each project (port of studio/clients/git.ts, ST-INV-06): status,
/// fetch, and the fast-forward of a clean main. No push, no other changes:
/// a shared work tree may hold someone else's work.
#include "source_sync.h"

#include "../common/json_util.h"
#include "ergo/studio/platform/process_runner.h"

#include <memory>

namespace ergo::studio::plugins::studio {

struct GitResult {
    bool        ok = false;
    std::string message;
};

struct SourceStatus {
    BranchStatus branch;
    std::string  repo_path;
    std::string  last_sha, last_subject, last_date;   ///< empty without commits
};
JsonValue to_json_value(const SourceStatus& s);

class GitClient {
public:
    GitClient(std::shared_ptr<platform::IProcessRunner> runner, int timeout_ms) : runner_(std::move(runner)), timeout_ms_(timeout_ms) {}

    /// nullopt with `err` when git fails.
    std::optional<SourceStatus> status(const std::string& repo, std::string& err) const;
    GitResult fetch(const std::string& repo) const;
    GitResult fast_forward(const std::string& repo, const std::string& branch) const;

private:
    platform::ProcessResult git(const std::vector<std::string>& args, int timeout_ms) const;

    std::shared_ptr<platform::IProcessRunner> runner_;
    int                                       timeout_ms_;
};

/// git's stderr (trimmed) or why it did not start.
std::string process_failure(const platform::ProcessResult& r, const std::string& what);

} // namespace ergo::studio::plugins::studio
