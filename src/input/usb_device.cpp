#include "ergo/input/usb_device.h"

namespace ergo::input {

const std::vector<uint8_t> UsbDevice::emptyReport_;

UsbDevice::UsbDevice() = default;
UsbDevice::~UsbDevice() { shutdown(); }

void UsbDevice::initialize() { initialized_ = true; }
void UsbDevice::shutdown() { initialized_ = false; }

void UsbDevice::poll() {
    // Platform-specific HID polling would go here.
}

void UsbDevice::swapBuffers() {
    for (DeviceIndex i = 0; i < kMaxDevices; ++i) {
        if (!devices_[i].connected) continue;
        devices_[i].buffer.swap();
    }
}

bool UsbDevice::isConnected(DeviceIndex index) const {
    if (index >= kMaxDevices) return false;
    return devices_[index].connected;
}

const std::vector<uint8_t>& UsbDevice::rawReport(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return emptyReport_;
    return devices_[index].buffer.read().rawReport;
}

float UsbDevice::axisValue(uint32_t axis, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return 0.0f;
    auto& axes = devices_[index].buffer.read().axes;
    auto it = axes.find(axis);
    return it != axes.end() ? it->second : 0.0f;
}

bool UsbDevice::buttonState(uint32_t button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    auto& buttons = devices_[index].buffer.read().buttons;
    auto it = buttons.find(button);
    return it != buttons.end() ? it->second : false;
}

const HidDescriptor& UsbDevice::descriptor(DeviceIndex index) const {
    static const HidDescriptor empty{};
    if (index >= kMaxDevices || !devices_[index].connected) return empty;
    return devices_[index].descriptor;
}

Observer& UsbDevice::observer() { return observer_; }
InputBuffer& UsbDevice::inputBuffer() { return inputBuffer_; }

void UsbDevice::injectRawReport(const std::vector<uint8_t>& report, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().rawReport = report;
}

void UsbDevice::injectDescriptor(const HidDescriptor& desc, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].descriptor = desc;
}

void UsbDevice::injectAxis(uint32_t axis, float value, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().axes[axis] = value;
    InputEvent ev{DeviceType::UsbGeneric, index, EventType::Axis,
                  static_cast<uint16_t>(axis), value, 0, Clock::now()};
    observer_.notify(ev);
}

void UsbDevice::injectButton(uint32_t button, bool state, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().buttons[button] = state;
    InputEvent ev{DeviceType::UsbGeneric, index,
                  state ? EventType::Press : EventType::Release,
                  static_cast<uint16_t>(button), state ? 1.0f : 0.0f, 0, Clock::now()};
    inputBuffer_.push(ev);
    observer_.notify(ev);
}

void UsbDevice::setConnected(bool connected, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].connected = connected;
}

} // namespace ergo::input
