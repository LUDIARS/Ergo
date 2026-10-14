#include "process.h"

#include <cstdio>
#include <cstdlib>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace ergo::package_cli {

namespace {

#if defined(_WIN32)
constexpr const char* kNullRedirect = " 2>nul";
constexpr const char* kAllNullRedirect = " >nul 2>nul";
#define ERGO_PKG_POPEN _popen
#define ERGO_PKG_PCLOSE _pclose
#else
constexpr const char* kNullRedirect = " 2>/dev/null";
constexpr const char* kAllNullRedirect = " >/dev/null 2>&1";
#define ERGO_PKG_POPEN popen
#define ERGO_PKG_PCLOSE pclose
#endif

std::string command_line(const std::vector<std::string>& argv) {
    std::string line;
    for (const auto& a : argv) {
        if (!line.empty()) line += ' ';
        line += '"' + a + '"';
    }
    return line;
}

/// cmd /c は先頭と末尾の引用符を剥がすので、 Windows では全体をもう 1 組で包む。
std::string shell_line(const std::string& line) {
#if defined(_WIN32)
    return "\"" + line + "\"";
#else
    return line;
#endif
}

bool all_safe(const std::vector<std::string>& argv) {
    for (const auto& a : argv) {
        if (!quote_safe(a)) return false;
    }
    return !argv.empty();
}

} // namespace

bool quote_safe(const std::string& value) {
    if (value.empty()) return false;
    for (const char c : value) {
        if (c == '"' || c == '\n' || c == '\r' || c == '%' || c == '`' || c == '$') return false;
    }
    return true;
}

bool capture(const std::vector<std::string>& argv, std::string& out) {
    return capture_status(argv, out) == 0;
}

int capture_status(const std::vector<std::string>& argv, std::string& out) {
    out.clear();
    if (!all_safe(argv)) return -1;
    FILE* pipe = ERGO_PKG_POPEN(shell_line(command_line(argv) + kNullRedirect).c_str(), "r");
    if (!pipe) return -1;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
    const int status = ERGO_PKG_PCLOSE(pipe);
#if defined(_WIN32)
    return status;
#else
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

bool run(const std::vector<std::string>& argv) {
    if (!all_safe(argv)) return false;
    std::fflush(stdout);
    return std::system(shell_line(command_line(argv)).c_str()) == 0;
}

int status(const std::vector<std::string>& argv) {
    if (!all_safe(argv)) return -1;
    std::fflush(stdout);
    const int rc = std::system(shell_line(command_line(argv) + kAllNullRedirect).c_str());
#if defined(_WIN32)
    return rc;
#else
    return (rc != -1 && WIFEXITED(rc)) ? WEXITSTATUS(rc) : -1;
#endif
}

std::string git_head(const std::string& dir) {
    std::string out;
    if (!capture({"git", "-C", dir, "rev-parse", "HEAD"}, out)) return {};
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    return out;
}

} // namespace ergo::package_cli
