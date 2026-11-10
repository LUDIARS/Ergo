#pragma once
/// Building an AI instruction (Cc delegation invoke), port of
/// studio/domain/delegation-request.ts (ST-INV-04 / ST-INV-05). Pure.
///   - cwd is the Cc project's repo_path; the editor cannot pass a path.
///   - missing required template inputs refuse the request; target_repo
///     defaults to repo_path; inputs outside input_schema are not sent.
///   - from an Actio task: taskflow_reference = "actio:<id>" (no new Cc task).
#include "../common/json_util.h"

#include <map>
#include <optional>
#include <string>

namespace ergo::studio::plugins::studio {

struct InstructionInput {
    std::string                call_name;
    JsonValue                  args = JsonValue::make_object();
    std::string                extra_prompt;
    std::optional<std::string> actio_task_id;
    /// Request text (an Actio task): into the template's `task` input when it
    /// has one, else at the head of the extra prompt.
    std::string                task_text;
};

struct BuildResult {
    bool        ok = false;
    JsonValue   request;   ///< Cc invoke body
    std::string reason;
};

constexpr std::size_t kMaxExtraPrompt = 20000;   ///< characters (code points)

/// `project` / `template_` are Cc records ({code, repo_path, ...} / {call_name, input_schema, ...}).
BuildResult build_invoke_request(const JsonValue& project, const JsonValue& template_, const InstructionInput& in,
                                 const std::string& triggered_by);

/// An Actio task as the instruction text.
std::string task_prompt(const std::string& id, const std::string& title, const std::string& description);

/// Whether a delegation run was sent for the project (cwd or target_repo is
/// its repo_path; separators and case ignored).
bool run_belongs_to(const JsonValue& run, const std::string& repo_path);

/// "git@github.com:Owner/Repo.git" / "https://github.com/Owner/Repo(.git)" -> "Owner/Repo".
std::optional<std::string> repo_slug_from_origin(const std::string& origin);

} // namespace ergo::studio::plugins::studio
