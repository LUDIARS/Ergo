#include "process.h"
#include "process_launch.h"

namespace ergo::package_cli {

namespace {

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
        if (c == '\0' || c == '"' || c == '\n' || c == '\r' || c == '%' || c == '`' || c == '$') return false;
    }
    return true;
}

bool capture(const std::vector<std::string>& argv, std::string& out) {
    return capture_status(argv, out) == 0;
}

int capture_status(const std::vector<std::string>& argv, std::string& out) {
    out.clear();
    if (!all_safe(argv)) return -1;
    return launch(argv, StreamMode::capture, StreamMode::discard, &out);
}

bool run(const std::vector<std::string>& argv) {
    if (!all_safe(argv)) return false;
    return launch(argv, StreamMode::inherit, StreamMode::inherit, nullptr) == 0;
}

int status(const std::vector<std::string>& argv) {
    if (!all_safe(argv)) return -1;
    return launch(argv, StreamMode::discard, StreamMode::discard, nullptr);
}

std::string git_head(const std::string& dir) {
    std::string out;
    if (!capture({"git", "-C", dir, "rev-parse", "HEAD"}, out)) return {};
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r' || out.back() == ' ')) out.pop_back();
    return out;
}

} // namespace ergo::package_cli
