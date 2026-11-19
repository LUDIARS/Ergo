// Windows の launch: CreateProcessW に argv をクォートしたコマンドラインで渡す (cmd.exe を挟まない)。
#include "process_launch.h"
#include "windows_command_line.h"

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <limits>
#include <optional>

namespace ergo::package_cli {

namespace {

namespace fs = std::filesystem;

std::wstring widen(const std::string& s) {
    if (s.empty() || s.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    const int size = static_cast<int>(s.size());
    const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), size, nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s.data(), size, w.data(), n) != n) return {};
    return w;
}

std::wstring env(const wchar_t* name) {
    const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    const DWORD n = GetEnvironmentVariableW(name, value.data(), size);
    if (n == 0 || n >= size) return {};
    value.resize(n);
    return value;
}

std::wstring lower(std::wstring s) {
    for (wchar_t& c : s) {
        if (c >= L'A' && c <= L'Z') c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return s;
}

/// 直接起動できる実行ファイルか (.bat / .cmd は cmd.exe を通るので受けない)。
bool launchable(const fs::path& p) {
    std::error_code ec;
    if (!fs::is_regular_file(p, ec)) return false;
    const std::wstring ext = lower(p.extension().wstring());
    return ext == L".exe" || ext == L".com";
}

std::optional<fs::path> with_extension(const fs::path& base) {
    if (base.has_extension() && launchable(base)) return base;
    for (const wchar_t* ext : {L".exe", L".com"}) {
        fs::path c = base;
        c += ext;
        if (launchable(c)) return c;
    }
    return std::nullopt;
}

/// argv[0] を実行ファイルへ解決する。 区切りを含めばそのパス、 含まなければ PATH だけを探す (カレントは探さない)。
std::optional<fs::path> resolve_program(const std::string& program) {
    const std::wstring wide_program = widen(program);
    if (wide_program.empty()) return std::nullopt;
    const fs::path p(wide_program);
    if (p.has_parent_path()) return with_extension(p);
    const std::wstring path = env(L"PATH");
    std::size_t start = 0;
    while (start <= path.size()) {
        const auto end = path.find(L';', start);
        std::wstring dir = path.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (dir.size() >= 2 && dir.front() == L'"' && dir.back() == L'"') {
            dir = dir.substr(1, dir.size() - 2);
        }
        if (!dir.empty()) {
            if (auto hit = with_extension(fs::path(dir) / p)) return hit;
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return std::nullopt;
}

/// 子に継承させるハンドル。 閉じ忘れないよう所有する。
class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE h) : h_(h == INVALID_HANDLE_VALUE ? nullptr : h) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    ~Handle() { reset(); }
    HANDLE get() const { return h_; }
    HANDLE* out() { reset(); return &h_; }
    /// 失敗値 (nullptr / INVALID_HANDLE_VALUE) は空として持つ。
    bool assign(HANDLE h) {
        reset();
        h_ = h == INVALID_HANDLE_VALUE ? nullptr : h;
        return h_ != nullptr;
    }
    void reset() {
        if (h_) CloseHandle(h_);
        h_ = nullptr;
    }

private:
    HANDLE h_ = nullptr;
};

class InheritedHandles {
public:
    InheritedHandles() = default;
    InheritedHandles(const InheritedHandles&) = delete;
    InheritedHandles& operator=(const InheritedHandles&) = delete;
    ~InheritedHandles() { if (list_) DeleteProcThreadAttributeList(list_); }

    bool init(HANDLE (&handles)[3]) {
        SIZE_T size = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
        if (size == 0) return false;
        storage_.resize(size);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) return false;
        list_ = list;
        return UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, handles,
                                         sizeof(handles), nullptr, nullptr) != FALSE;
    }

    LPPROC_THREAD_ATTRIBUTE_LIST get() const { return list_; }

private:
    std::vector<unsigned char> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

HANDLE open_null(SECURITY_ATTRIBUTES* sa, DWORD access) {
    return CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE, sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

/// 親の標準ハンドルを、 子が継承できる複製にする。
HANDLE inheritable_std(DWORD which) {
    const HANDLE h = GetStdHandle(which);
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return nullptr;
    HANDLE dup = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), h, GetCurrentProcess(), &dup, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
        return nullptr;
    }
    return dup;
}

/// 従来のテキスト取り込みと同じく CRLF を LF にそろえる。
std::string crlf_to_lf(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == '\r' && i + 1 < raw.size() && raw[i + 1] == '\n') continue;
        out.push_back(raw[i]);
    }
    return out;
}

/// 親の標準ハンドルを継承させる。 親が持たない (デタッチ等) ときは null デバイスにする。
bool inherit_or_null(Handle& h, DWORD which, SECURITY_ATTRIBUTES* sa, DWORD access) {
    const HANDLE standard = GetStdHandle(which);
    if (standard == nullptr || standard == INVALID_HANDLE_VALUE) return h.assign(open_null(sa, access));
    return h.assign(inheritable_std(which));
}

} // namespace

int launch(const std::vector<std::string>& argv, StreamMode out_mode, StreamMode err_mode, std::string* captured) {
    if (argv.empty() || err_mode == StreamMode::capture) return -1;
    if (out_mode == StreamMode::capture && captured == nullptr) return -1;
    const auto exe = resolve_program(argv.front());
    if (!exe) return -1;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    Handle child_in, child_out, child_err, read_out;
    if (!inherit_or_null(child_in, STD_INPUT_HANDLE, &sa, GENERIC_READ)) return -1;
    switch (out_mode) {
        case StreamMode::inherit:
            if (!inherit_or_null(child_out, STD_OUTPUT_HANDLE, &sa, GENERIC_WRITE)) return -1;
            break;
        case StreamMode::discard:
            if (!child_out.assign(open_null(&sa, GENERIC_WRITE))) return -1;
            break;
        case StreamMode::capture:
            if (!CreatePipe(read_out.out(), child_out.out(), &sa, 0)) return -1;
            if (!SetHandleInformation(read_out.get(), HANDLE_FLAG_INHERIT, 0)) return -1;
            break;
    }
    const bool err_ok = err_mode == StreamMode::inherit ? inherit_or_null(child_err, STD_ERROR_HANDLE, &sa, GENERIC_WRITE)
                                                        : child_err.assign(open_null(&sa, GENERIC_WRITE));
    if (!err_ok) return -1;

    // Only the intended standard handles may reach this child, even with concurrent launches.
    HANDLE handles[] = {child_in.get(), child_out.get(), child_err.get()};
    InheritedHandles inherited;
    if (!inherited.init(handles)) return -1;
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = child_in.get();
    si.StartupInfo.hStdOutput = child_out.get();
    si.StartupInfo.hStdError = child_err.get();
    si.lpAttributeList = inherited.get();

    // 子の argv[0] は解決したパスにする (引数の分け方は windows_command_line の規則どおり)。
    std::vector<std::string> args = argv;
    args.front() = exe->u8string();
    std::wstring cmdline = widen(windows_command_line(args));
    if (cmdline.empty()) return -1;
    const std::wstring app = exe->wstring();

    std::fflush(stdout);
    std::fflush(stderr);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(app.c_str(), cmdline.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
                                   nullptr, nullptr, &si.StartupInfo, &pi);
    // 子の端は親で閉じる (閉じないとパイプが EOF にならない)。
    child_in.reset();
    child_out.reset();
    child_err.reset();
    if (!ok) return -1;
    Handle process(pi.hProcess);
    Handle thread(pi.hThread);

    bool read_ok = true;
    if (out_mode == StreamMode::capture) {
        std::string raw;
        char  buffer[4096];
        DWORD got = 0;
        // stderr は null か端末へ向けているので、 標準出力だけを読み切っても詰まらない。
        while (ReadFile(read_out.get(), buffer, sizeof(buffer), &got, nullptr)) {
            if (got > 0) raw.append(buffer, got);
        }
        read_ok = GetLastError() == ERROR_BROKEN_PIPE;
        read_out.reset();
        *captured = crlf_to_lf(raw);
    }
    if (WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0) return -1;
    DWORD code = 0;
    if (!read_ok || !GetExitCodeProcess(process.get(), &code)) return -1;
    return static_cast<int>(code);
}

} // namespace ergo::package_cli
