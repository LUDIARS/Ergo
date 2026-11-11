// libFuzzer entry: ergo::vector::parse_svg_data with small limits.
#include "ergo/vector/vector.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    using namespace ergo::vector;
    SvgLimits limits;
    limits.max_points = 4096;
    limits.max_shapes = 256;
    std::vector<VectorPath> out;
    if (parse_svg_data(std::string_view(reinterpret_cast<const char*>(data), size), limits, out)) {
        std::size_t points = 0;
        for (const auto& p : out) points += p.points.size();
        if (points > limits.max_points || out.size() > limits.max_shapes) std::abort();
    }
    return 0;
}
