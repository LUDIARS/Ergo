// usb_device_dummy.cpp
// Dummy plug implementation for UsbDevice module.

#include "ergo/input/usb_device.h"
#include <memory>

namespace ergo::input::dummy {

std::unique_ptr<UsbDevice> createDummyUsb() {
    auto device = std::make_unique<UsbDevice>();
    device->initialize();
    device->setConnected(true, 0);
    return device;
}

} // namespace ergo::input::dummy
