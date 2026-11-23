#pragma once
// Windows only: the CreateProcessW application name and command line for a
// program and its arguments, shared by the blocking runner and live processes.
// A .cmd / .bat program runs through cmd.exe /d /s /c, so its arguments must
// be cmd_safe_arg(); anything else runs directly.
#include <string>
#include <vector>

namespace ergo::studio::platform {

struct WinCommand {
    std::wstring app;       ///< lpApplicationName
    std::string  cmdline;   ///< UTF-8; widen before CreateProcessW
};

bool build_win_command(const std::string& program, const std::vector<std::string>& args, WinCommand& out,
                       std::string& err);

std::wstring widen_utf8(const std::string& s);

} // namespace ergo::studio::platform
