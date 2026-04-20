// input_buffer_dummy.cpp
// Dummy plug implementation for InputBuffer module.

#include "ergo/input/input_buffer.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<InputBuffer> createDummyInputBuffer() {
    return std::make_unique<InputBuffer>(256);
}

} // namespace ergo::input::dummy
