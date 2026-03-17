// double_buffer_dummy.cpp
// Dummy plug implementation for DoubleBuffer template.
// Minimal compilation unit since DoubleBuffer is header-only.

#include "ergo/input/double_buffer.h"

namespace ergo::input {

// Instantiate to verify the template compiles.
template class DoubleBuffer<int>;

} // namespace ergo::input
