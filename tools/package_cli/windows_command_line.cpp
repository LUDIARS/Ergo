#include "windows_command_line.h"

namespace ergo::package_cli {

std::string quote_windows_arg(const std::string& arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
    std::string out = "\"";
    std::size_t backslashes = 0;
    for (const char c : arg) {
        if (c == '\\') {
            ++backslashes;
        } else if (c == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
        } else {
            out.append(backslashes, '\\');
            out.push_back(c);
            backslashes = 0;
        }
    }
    out.append(backslashes * 2, '\\');   // 閉じ引用符の直前の \ は倍にする
    out.push_back('"');
    return out;
}

std::string windows_command_line(const std::vector<std::string>& argv) {
    std::string line;
    for (const auto& a : argv) {
        if (!line.empty()) line += ' ';
        line += quote_windows_arg(a);
    }
    return line;
}

} // namespace ergo::package_cli
