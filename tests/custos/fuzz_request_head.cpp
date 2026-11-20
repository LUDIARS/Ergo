// libFuzzer entry: ergo_custos's request head check (request_head.h).
#include "../../src/custos/request_head.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ergo::custos::detail;
    const std::string head(reinterpret_cast<const char*>(data), size);
    HttpRequest req;
    std::size_t length = 0;
    const RequestLimits limits;
    if (parse_request_head(head, limits, req, length) == HeadStatus::Ok && length > limits.max_body) std::abort();
    return 0;
}
