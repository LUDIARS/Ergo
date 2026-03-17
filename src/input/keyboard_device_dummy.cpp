// keyboard_device_dummy.cpp
// Dummy plug implementation for KeyboardDevice module.

#include "ergo/input/keyboard_device.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<KeyboardDevice> createDummyKeyboard() {
    auto device = std::make_unique<KeyboardDevice>();
    device->initialize();
    device->setConnected(true, 0);
    return device;
}

} // namespace ergo::input::dummy
