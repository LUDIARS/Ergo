// mouse_device_dummy.cpp
// Dummy plug implementation for MouseDevice module.

#include "ergo/input/mouse_device.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<MouseDevice> createDummyMouse() {
    auto device = std::make_unique<MouseDevice>();
    device->initialize();
    device->setConnected(true, 0);
    return device;
}

} // namespace ergo::input::dummy
