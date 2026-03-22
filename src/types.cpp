#include "ergo/network/types.h"
#include <algorithm>

namespace ergo::network {

void HttpRequest::set_header(const std::string& name, const std::string& value) {
    for (auto& h : headers) {
        if (h.name == name) {
            h.value = value;
            return;
        }
    }
    headers.push_back({name, value});
}

std::string HttpRequest::get_header(const std::string& name) const {
    for (const auto& h : headers) {
        if (h.name == name) {
            return h.value;
        }
    }
    return "";
}

std::string HttpResponse::get_header(const std::string& name) const {
    for (const auto& h : headers) {
        if (h.name == name) {
            return h.value;
        }
    }
    return "";
}

} // namespace ergo::network
