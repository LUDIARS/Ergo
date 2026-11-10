// Windows IProcessRunner: CreateProcessW with piped stdio.
#include "ergo/studio/platform/process_runner.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <thread>

namespace ergo::studio::platform {
namespace fs = std::filesystem;
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

/// The variable's value as UTF-8, "" when unset (read wide: no CRT copy to free).
std::string env(const char* name) {
    const std::wstring wide_name = widen(name);
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

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::size_t start = 0;
    while (start <= s.size()) {
        const auto end = s.find(sep, start);
        std::string part = s.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!part.empty()) out.push_back(part);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

/// Read a pipe to EOF on its own thread so a full stderr cannot block stdout.
void drain(HANDLE h, std::string* out) {
    char  buf[8192];
    DWORD got = 0;
    while (ReadFile(h, buf, sizeof(buf), &got, nullptr) && got > 0) out->append(buf, got);
}

class WinProcessRunner final : public IProcessRunner {
public:
    ProcessResult run(const ProcessSpec& spec) override {
        ProcessResult r;
        const auto exe = find_on_path(spec.program);
        if (!exe) {
            r.error = "program not found on PATH: " + spec.program;
            return r;
        }
        const std::string ext = lower(exe->extension().u8string());
        const bool        shim = ext == ".cmd" || ext == ".bat";

        std::string cmdline;
        std::wstring app;
        if (shim) {
            // cmd.exe re-parses the line: refuse anything it could act on.
            for (const auto& a : spec.args) {
                if (!cmd_safe_arg(a)) {
                    r.error = "argument not allowed for a .cmd program: " + a;
                    return r;
                }
            }
            const std::string comspec = env("ComSpec");
            if (comspec.empty()) {
                r.error = "ComSpec is not set";
                return r;
            }
            app     = widen(comspec);
            cmdline = "cmd.exe /d /s /c \"" + quote_windows_arg(exe->u8string());
            for (const auto& a : spec.args) cmdline += " " + quote_windows_arg(a);
            cmdline += "\"";
        } else {
            app     = exe->wstring();
            cmdline = quote_windows_arg(exe->u8string());
            for (const auto& a : spec.args) cmdline += " " + quote_windows_arg(a);
        }

        SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
        HANDLE in_r = nullptr, in_w = nullptr, out_r = nullptr, out_w = nullptr, err_r = nullptr, err_w = nullptr;
        if (!CreatePipe(&in_r, &in_w, &sa, 0) || !CreatePipe(&out_r, &out_w, &sa, 0) || !CreatePipe(&err_r, &err_w, &sa, 0)) {
            r.error = "CreatePipe failed";
            return r;
        }
        // The parent's ends must not leak into the child.
        SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(err_r, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb         = sizeof(si);
        si.dwFlags    = STARTF_USESTDHANDLES;
        si.hStdInput  = in_r;
        si.hStdOutput = out_w;
        si.hStdError  = err_w;
        PROCESS_INFORMATION pi{};
        std::wstring wcmd = widen(cmdline);
        const std::wstring wcwd = spec.cwd.empty() ? std::wstring() : spec.cwd.wstring();
        const BOOL ok = CreateProcessW(app.c_str(), wcmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                       wcwd.empty() ? nullptr : wcwd.c_str(), &si, &pi);
        CloseHandle(in_r);
        CloseHandle(out_w);
        CloseHandle(err_w);
        if (!ok) {
            r.error = "CreateProcess failed (error " + std::to_string(GetLastError()) + ")";
            CloseHandle(in_w);
            CloseHandle(out_r);
            CloseHandle(err_r);
            return r;
        }
        r.started = true;
        std::thread t_out(drain, out_r, &r.out);
        std::thread t_err(drain, err_r, &r.err);
        DWORD written = 0;
        if (!spec.stdin_text.empty()) {
            WriteFile(in_w, spec.stdin_text.data(), static_cast<DWORD>(spec.stdin_text.size()), &written, nullptr);
        }
        CloseHandle(in_w);

        if (WaitForSingleObject(pi.hProcess, static_cast<DWORD>(spec.timeout_ms)) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            r.timed_out = true;
            r.error     = "timed out after " + std::to_string(spec.timeout_ms) + " ms";
        }
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        r.exit_code = static_cast<int>(code);
        t_out.join();
        t_err.join();
        CloseHandle(out_r);
        CloseHandle(err_r);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return r;
    }
};

} // namespace

std::optional<fs::path> find_on_path(const std::string& program) {
    const fs::path p = fs::u8path(program);
    std::vector<std::string> exts = split(env("PATHEXT"), ';');
    if (exts.empty()) exts = {".COM", ".EXE", ".BAT", ".CMD"};
    auto try_file = [&](const fs::path& base) -> std::optional<fs::path> {
        std::error_code ec;
        if (base.has_extension() && fs::is_regular_file(base, ec)) return base;
        for (const auto& e : exts) {
            fs::path c = base;
            c += fs::u8path(e);
            if (fs::is_regular_file(c, ec)) return c;
        }
        return std::nullopt;
    };
    if (p.has_parent_path()) return try_file(p);
    for (const auto& dir : split(env("PATH"), ';')) {
        if (auto hit = try_file(fs::u8path(dir) / p)) return hit;
    }
    return std::nullopt;
}

std::shared_ptr<IProcessRunner> make_system_process_runner() { return std::make_shared<WinProcessRunner>(); }

} // namespace ergo::studio::platform
