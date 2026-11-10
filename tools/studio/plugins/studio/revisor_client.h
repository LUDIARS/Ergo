#pragma once
/// Revisor: local PR list, read only (port of studio/clients/revisor.ts), via
/// `node <LUDIARS_ROOT>/Revisor/src/cli.mjs pr list --json`. Submitting and
/// merging stay with Cc's workflow and people (ST-INV-06).
#include "../common/json_util.h"
#include "ergo/studio/platform/process_runner.h"

#include <memory>
#include <optional>

namespace ergo::studio::plugins::studio {

class RevisorClient {
public:
    RevisorClient(std::shared_ptr<platform::IProcessRunner> runner, std::optional<std::string> ludiars_root, int timeout_ms);

    /// The CLI path, nullopt when LUDIARS_ROOT is unset or Revisor is not cloned.
    std::optional<std::string> cli() const;
    bool available() const { return cli().has_value(); }

    /// Lighter connectivity check (repo list). "" when ok, else why not.
    std::string ping() const;
    /// [{number, repository, title, headRef, baseRef, status, checkStatus, updatedAt?, createdAt?}];
    /// nullopt with `err` on failure.
    std::optional<JsonValue> prs(const std::string& repository, std::string& err) const;

private:
    platform::ProcessResult node(const std::vector<std::string>& args) const;

    std::shared_ptr<platform::IProcessRunner> runner_;
    std::optional<std::string>                root_;
    int                                       timeout_ms_;
};

} // namespace ergo::studio::plugins::studio
