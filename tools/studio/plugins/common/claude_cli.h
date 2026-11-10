#pragma once
/// One-shot `claude -p` calls for plugins that ask an LLM (vfx_suggest,
/// vfx_breed) — design J7: the Studio holds no API key; the prompt goes on
/// stdin (long prompts break on the command line) and the reply comes back
/// in the CLI's --output-format json envelope ({"result": "..."}).
#include "ergo/studio/platform/process_runner.h"

#include <memory>
#include <string>

namespace ergo::studio::plugins {

/// Model names passed to the CLI: [A-Za-z0-9._:-]+ (never a shell fragment).
bool is_safe_model_name(const std::string& model);

class ClaudeCli {
public:
    /// `bin` = program name or path (CLAUDE_BIN, "claude" by default),
    /// `model` = CLI model alias or id.
    ClaudeCli(std::shared_ptr<platform::IProcessRunner> runner, std::string bin, std::string model, int timeout_ms = 180000);

    const std::string& model() const { return model_; }
    /// "" when calls can be made, otherwise why not (bad model name, CLI not on PATH).
    std::string        not_ready_reason() const;

    struct Reply {
        bool        ok = false;
        std::string text;    ///< the envelope's "result" (or raw stdout when there is no envelope)
        std::string error;
    };
    Reply ask(const std::string& prompt) const;

private:
    std::shared_ptr<platform::IProcessRunner> runner_;
    std::string                               bin_;
    std::string                               model_;
    int                                       timeout_ms_;
};

} // namespace ergo::studio::plugins
