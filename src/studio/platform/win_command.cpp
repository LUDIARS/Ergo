// Windows: program + arguments -> CreateProcessW inputs (see win_command.h).
#include "win_command.h"

#include "ergo/studio/platform/process_runner.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <cctype>

namespace ergo::studio::platform {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

/// The variable's value as UTF-8, "" when unset.
std::string env_utf8(const char* name) {
    const std::wstring wide_name = widen_utf8(name);
    const DWORD size = GetEnvironmentVariableW(wide_name.c_str(), nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, wchar_t{});
    const DWORD n = GetEnvironmentVariableW(wide_name.c_str(), value.data(), size);
    if (n == 0 || n >= size) return {};
    value.resize(n);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(bytes), char{});
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), out.data(), bytes, nullptr, nullptr);
    return out;
}

} // namespace

std::wstring widen_utf8(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

bool build_win_command(const std::string& program, const std::vector<std::string>& args, WinCommand& out,
                       std::string& err) {
    const auto exe = find_on_path(program);
    if (!exe) {
        err = "program not found on PATH: " + program;
        return false;
    }
    const std::string ext = lower(exe->extension().u8string());
    if (ext == ".cmd" || ext == ".bat") {
        // cmd.exe re-parses the line: refuse anything it could act on.
        for (const auto& a : args) {
            if (!cmd_safe_arg(a)) {
                err = "argument not allowed for a .cmd program: " + a;
                return false;
            }
        }
        const std::string comspec = env_utf8("ComSpec");
        if (comspec.empty()) {
            err = "ComSpec is not set";
            return false;
        }
        out.app     = widen_utf8(comspec);
        out.cmdline = "cmd.exe /d /s /c \"" + quote_windows_arg(exe->u8string());
        for (const auto& a : args) out.cmdline += " " + quote_windows_arg(a);
        out.cmdline += "\"";
        return true;
    }
    out.app     = exe->wstring();
    out.cmdline = quote_windows_arg(exe->u8string());
    for (const auto& a : args) out.cmdline += " " + quote_windows_arg(a);
    return true;
}

} // namespace ergo::studio::platform
