#include "ergo/studio/host/plugin.h"

namespace ergo::studio::host {
namespace {
bool lower_alnum(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }
} // namespace

bool is_valid_plugin_id(const std::string& id) {
    if (id.empty() || !lower_alnum(id[0])) return false;
    for (char c : id) {
        if (!lower_alnum(c) && c != '_' && c != '-') return false;
    }
    return true;
}

bool is_reserved_plugin_id(const std::string& id) { return id == "api" || id == "shell"; }

} // namespace ergo::studio::host
