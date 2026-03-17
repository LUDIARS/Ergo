#include "ergo/input/mouse_device.h"

namespace ergo::input {

MouseDevice::MouseDevice() = default;
MouseDevice::~MouseDevice() { shutdown(); }

void MouseDevice::initialize() { initialized_ = true; }
void MouseDevice::shutdown() { initialized_ = false; }

void MouseDevice::poll() {
    // Platform-specific polling would go here.
    // In tests we use inject* methods instead.
}

void MouseDevice::swapBuffers() {
    for (DeviceIndex i = 0; i < kMaxDevices; ++i) {
        if (!devices_[i].connected) continue;
        auto& dev = devices_[i];
        dev.prevState = dev.buffer.read();
        dev.buffer.swap();
    }
}

bool MouseDevice::isConnected(DeviceIndex index) const {
    if (index >= kMaxDevices) return false;
    return devices_[index].connected;
}

bool MouseDevice::isButtonDown(MouseButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    return (devices_[index].buffer.read().buttons >> static_cast<uint8_t>(button)) & 1;
}

bool MouseDevice::isButtonPressed(MouseButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    uint8_t bit = static_cast<uint8_t>(button);
    bool cur = (devices_[index].buffer.read().buttons >> bit) & 1;
    bool prev = (devices_[index].prevState.buttons >> bit) & 1;
    return cur && !prev;
}

bool MouseDevice::isButtonReleased(MouseButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    uint8_t bit = static_cast<uint8_t>(button);
    bool cur = (devices_[index].buffer.read().buttons >> bit) & 1;
    bool prev = (devices_[index].prevState.buttons >> bit) & 1;
    return !cur && prev;
}

Vec2f MouseDevice::cursorPosition(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return {};
    if (devices_[index].cursorLocked) return devices_[index].lockedPosition;
    return devices_[index].buffer.read().position;
}

Vec2f MouseDevice::moveDelta(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return {};
    return devices_[index].buffer.read().delta;
}

Vec2f MouseDevice::scrollDelta(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return {};
    return devices_[index].buffer.read().scroll;
}

void MouseDevice::setCursorLock(bool locked, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].cursorLocked = locked;
    if (locked) {
        devices_[index].lockedPosition = devices_[index].buffer.read().position;
    }
}

bool MouseDevice::isCursorLocked(DeviceIndex index) const {
    if (index >= kMaxDevices) return false;
    return devices_[index].cursorLocked;
}

Observer& MouseDevice::observer() { return observer_; }
InputBuffer& MouseDevice::inputBuffer() { return inputBuffer_; }

void MouseDevice::injectButtonState(uint8_t buttons, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    auto& state = devices_[index].buffer.writableRef();
    uint8_t oldButtons = state.buttons;
    state.buttons = buttons;

    for (uint8_t b = 0; b < static_cast<uint8_t>(MouseButton::Count); ++b) {
        bool wasDown = (oldButtons >> b) & 1;
        bool isDown = (buttons >> b) & 1;
        if (isDown && !wasDown) {
            InputEvent ev{DeviceType::Mouse, index, EventType::Press, b, 1.0f, 0, Clock::now()};
            inputBuffer_.push(ev);
            observer_.notify(ev);
        } else if (!isDown && wasDown) {
            InputEvent ev{DeviceType::Mouse, index, EventType::Release, b, 0.0f, 0, Clock::now()};
            inputBuffer_.push(ev);
            observer_.notify(ev);
        }
    }
}

void MouseDevice::injectPosition(Vec2f pos, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    auto& state = devices_[index].buffer.writableRef();
    state.delta = {pos.x - state.position.x, pos.y - state.position.y};
    state.position = pos;
}

void MouseDevice::injectScroll(Vec2f scroll, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().scroll = scroll;
}

void MouseDevice::setConnected(bool connected, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].connected = connected;
}

} // namespace ergo::input
