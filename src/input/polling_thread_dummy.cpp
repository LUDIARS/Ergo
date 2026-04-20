// polling_thread_dummy.cpp
// Dummy plug implementation for PollingThread module.

#include "ergo/input/polling_thread.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<PollingThread> createDummyPollingThread() {
    return std::make_unique<PollingThread>();
}

} // namespace ergo::input::dummy
