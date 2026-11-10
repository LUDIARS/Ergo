#include "ergo/studio/platform/process_runner.h"

namespace ergo::studio::platform {

std::string quote_windows_arg(const std::string& arg) {
    if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
    std::string out = "\"";
    std::size_t backslashes = 0;
    for (char c : arg) {
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
    out.append(backslashes * 2, '\\');   // before the closing quote
    out.push_back('"');
    return out;
}

bool cmd_safe_arg(const std::string& arg) {
    for (unsigned char c : arg) {
        if (c < 0x20 || c == 0x7F) return false;
        switch (c) {
            case '&': case '|': case '<': case '>': case '^': case '%': case '!': case '"': case '(': case ')':
                return false;
            default: break;
        }
    }
    return true;
}

} // namespace ergo::studio::platform
