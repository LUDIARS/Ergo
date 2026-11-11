#include "ergo/studio/host/plugin.h"

namespace ergo::studio::host {

std::string expand_usage_hint(std::string_view hint, std::string_view addr) {
    constexpr std::string_view kToken = "{addr}";
    std::string out;
    std::size_t from = 0;
    for (std::size_t at = hint.find(kToken); at != std::string::npos; at = hint.find(kToken, from)) {
        out.append(hint.substr(from, at - from)).append(addr);
        from = at + kToken.size();
    }
    return out.append(hint.substr(from));
}

} // namespace ergo::studio::host
