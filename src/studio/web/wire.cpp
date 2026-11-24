#include "wire.h"

#include "ergo/common/json_min.h"

#include <portus/http_message.hpp>

namespace ergo::studio::web {

std::string wire_response(const net::HttpResponse& res, bool head_only) {
    const bool  no_body = res.status == 204 || res.status == 304;
    std::string out     = "HTTP/1.1 " + std::to_string(res.status) + " " + portus::status_text(res.status) + "\r\n";
    for (const auto& [name, value] : res.headers) {
        if (name == "content-length" || name == "connection" || name == "transfer-encoding") continue;
        if (!portus::is_valid_header_name(name) || !portus::is_valid_header_value(value)) continue;
        out += name + ": " + value + "\r\n";
    }
    if (!no_body) out += "content-length: " + std::to_string(res.body.size()) + "\r\n";
    out += "connection: close\r\n\r\n";
    if (!no_body && !head_only) out += res.body;
    return out;
}

std::string wire_refusal(int status, const std::string& why) {
    namespace jsonm = ergo::common::jsonm;
    jsonm::JsonValue body = jsonm::JsonValue::make_object();
    body.set("ok", jsonm::JsonValue::make_bool(false));
    body.set("err", jsonm::JsonValue::make_string(why));
    return wire_response(net::HttpResponse::json(status, jsonm::serialize(body)));
}

} // namespace ergo::studio::web
