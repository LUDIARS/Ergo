// input_system_dummy.cpp
// Dummy plug implementation for InputSystem facade.

#include "ergo/input/input_system.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<InputSystem> createDummyInputSystem() {
    auto system = std::make_unique<InputSystem>();
    InputConfig config;
    config.threadMode = ThreadMode::MainSync;
    config.enableMouse = true;
    config.enableKeyboard = true;
    config.enableGamepad = true;
    config.enableUsb = true;
    system->initialize(config);
    return system;
}

} // namespace ergo::input::dummy
