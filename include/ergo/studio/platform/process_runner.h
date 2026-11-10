#pragma once
/// Run a child process to completion with stdin fed and stdout / stderr
/// captured (git, revisor, `claude -p`). Arguments are passed as a vector and
/// quoted by the runner; nothing goes through a shell except a Windows
/// .cmd / .bat shim, which cmd.exe must interpret (npm installs `claude` as
/// one) — then every argument must be free of cmd metacharacters.
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ergo::studio::platform {

struct ProcessSpec {
    std::string              program;   ///< name looked up on PATH, or a path
    std::vector<std::string> args;
    std::string              stdin_text;
    std::filesystem::path    cwd;       ///< empty = inherit
    int                      timeout_ms = 120000;
};

struct ProcessResult {
    bool        started   = false;
    bool        timed_out = false;
    int         exit_code = -1;
    std::string out, err;
    std::string error;   ///< why it did not start / finish
};

class IProcessRunner {
public:
    virtual ~IProcessRunner() = default;
    /// Blocking. Safe from several threads.
    virtual ProcessResult run(const ProcessSpec& spec) = 0;
};

std::shared_ptr<IProcessRunner> make_system_process_runner();

/// PATH lookup with PATHEXT on Windows; nullopt when not found.
std::optional<std::filesystem::path> find_on_path(const std::string& program);

/// Windows command-line quoting of one argument (CommandLineToArgvW rules).
std::string quote_windows_arg(const std::string& arg);

/// True when `arg` can be handed to cmd.exe unchanged (no & | < > ^ % ! " ( ) and no control chars).
bool cmd_safe_arg(const std::string& arg);

} // namespace ergo::studio::platform
