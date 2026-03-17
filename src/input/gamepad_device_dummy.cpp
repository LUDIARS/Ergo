// gamepad_device_dummy.cpp
// Dummy plug implementation for GamepadDevice module.

#include "ergo/input/gamepad_device.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<GamepadDevice> createDummyGamepad() {
    auto device = std::make_unique<GamepadDevice>();
    device->initialize();
    device->setConnected(true, 0);
    return device;
}

} // namespace ergo::input::dummy
