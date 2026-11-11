// libFuzzer entry: ergo::common::jsonm::parse (default and tight limits) and a
// serialize -> parse round trip of what parsed.
#include "ergo/common/json_min.h"

#include <cstddef>
#include <cstdint>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ergo::common::jsonm;
    const std::string src(reinterpret_cast<const char*>(data), size);
    JsonValue v;
    if (parse(src, v)) {
        // Not asserted: a number out of double range parses as inf and
        // serializes as "inf", which parse refuses (spec/setup/hardening.md).
        JsonValue again;
        (void)parse(serialize(v), again);
    }
    ParseLimits tight;
    tight.max_values = 64;
    tight.max_depth  = 4;
    JsonValue small;
    (void)parse(src, small, tight);
    return 0;
}
