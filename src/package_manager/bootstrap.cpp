#include "ergo/package_manager/bootstrap.h"

namespace ergo::package_manager {

const char* const kBootstrapPlaceholder = "@ERGO_BOOTSTRAP_SOURCE@";
const char* const kBootstrapPath        = "cmake/ergo-bootstrap.cmake";

namespace {

const char* const kSourceMarker = "# ergo-bootstrap-source: ";

std::string to_lf(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        if (c != '\r') out += c;
    }
    return out;
}

} // namespace

std::string render_bootstrap(const std::string& tmpl, const std::string& source_sha) {
    std::string out = to_lf(tmpl);
    const std::string placeholder = kBootstrapPlaceholder;
    for (std::size_t at = out.find(placeholder); at != std::string::npos;
         at = out.find(placeholder, at + source_sha.size())) {
        out.replace(at, placeholder.size(), source_sha);
    }
    return out;
}

std::string bootstrap_source(const std::string& text) {
    const std::string marker = kSourceMarker;
    const std::size_t at = text.find(marker);
    if (at == std::string::npos) return {};
    const std::size_t begin = at + marker.size();
    std::size_t end = text.find_first_of("\r\n", begin);
    if (end == std::string::npos) end = text.size();
    return text.substr(begin, end - begin);
}

const char* bootstrap_state_name(BootstrapState s) {
    switch (s) {
    case BootstrapState::Missing: return "missing";
    case BootstrapState::Current: return "current";
    case BootstrapState::Stale:   return "stale";
    case BootstrapState::Foreign: return "foreign";
    }
    return "missing";
}

BootstrapStatus check_bootstrap(const std::string* text, const std::string& tmpl) {
    BootstrapStatus status;
    if (!text) return status;
    status.source = bootstrap_source(*text);
    if (status.source.empty()) {
        status.state = BootstrapState::Foreign;
        return status;
    }
    status.state = to_lf(*text) == render_bootstrap(tmpl, status.source) ? BootstrapState::Current
                                                                           : BootstrapState::Stale;
    return status;
}

} // namespace ergo::package_manager
