#include "ergo/input/gamepad_device.h"
#include <cmath>
#include <algorithm>

namespace ergo::input {

GamepadDevice::GamepadDevice() = default;
GamepadDevice::~GamepadDevice() { shutdown(); }

void GamepadDevice::initialize() { initialized_ = true; }
void GamepadDevice::shutdown() { initialized_ = false; }

void GamepadDevice::poll() {
    // Platform-specific polling would go here.
}

void GamepadDevice::swapBuffers() {
    for (DeviceIndex i = 0; i < kMaxDevices; ++i) {
        if (!devices_[i].connected) continue;
        auto& dev = devices_[i];
        dev.prevState = dev.buffer.read();
        dev.buffer.swap();
    }
}

bool GamepadDevice::isConnected(DeviceIndex index) const {
    if (index >= kMaxDevices) return false;
    return devices_[index].connected;
}

bool GamepadDevice::isButtonDown(GamepadButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    return (devices_[index].buffer.read().buttons >> static_cast<uint16_t>(button)) & 1;
}

bool GamepadDevice::isButtonPressed(GamepadButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    uint16_t bit = static_cast<uint16_t>(button);
    bool cur = (devices_[index].buffer.read().buttons >> bit) & 1;
    bool prev = (devices_[index].prevState.buttons >> bit) & 1;
    return cur && !prev;
}

bool GamepadDevice::isButtonReleased(GamepadButton button, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    uint16_t bit = static_cast<uint16_t>(button);
    bool cur = (devices_[index].buffer.read().buttons >> bit) & 1;
    bool prev = (devices_[index].prevState.buttons >> bit) & 1;
    return !cur && prev;
}

float GamepadDevice::axisValue(uint8_t axis, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected || axis >= kMaxAxes) return 0.0f;
    return applyDeadZone(devices_[index].buffer.read().axes[axis], devices_[index].deadZone);
}

Vec2f GamepadDevice::leftStick(DeviceIndex index) const {
    return {axisValue(0, index), axisValue(1, index)};
}

Vec2f GamepadDevice::rightStick(DeviceIndex index) const {
    return {axisValue(2, index), axisValue(3, index)};
}

float GamepadDevice::leftTrigger(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return 0.0f;
    return std::clamp(devices_[index].buffer.read().axes[4], 0.0f, 1.0f);
}

float GamepadDevice::rightTrigger(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return 0.0f;
    return std::clamp(devices_[index].buffer.read().axes[5], 0.0f, 1.0f);
}

void GamepadDevice::setDeadZone(float deadZone, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].deadZone = deadZone;
}

float GamepadDevice::deadZone(DeviceIndex index) const {
    if (index >= kMaxDevices) return 0.0f;
    return devices_[index].deadZone;
}

void GamepadDevice::setVibration(VibrationParams params, DeviceIndex index) {
    if (index >= kMaxDevices || !devices_[index].connected) return;
    devices_[index].vibration = params;
    // Platform-specific vibration API call would go here.
}

float GamepadDevice::batteryLevel(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return 0.0f;
    return devices_[index].batteryLevel;
}

bool GamepadDevice::isWireless(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    return devices_[index].wireless;
}

Observer& GamepadDevice::observer() { return observer_; }
InputBuffer& GamepadDevice::inputBuffer() { return inputBuffer_; }

void GamepadDevice::injectButtonState(uint32_t buttons, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    auto& state = devices_[index].buffer.writableRef();
    uint32_t oldButtons = state.buttons;
    state.buttons = buttons;

    for (uint16_t b = 0; b < static_cast<uint16_t>(GamepadButton::Count); ++b) {
        bool wasDown = (oldButtons >> b) & 1;
        bool isDown = (buttons >> b) & 1;
        if (isDown && !wasDown) {
            InputEvent ev{DeviceType::Gamepad, index, EventType::Press, b, 1.0f, 0, Clock::now()};
            inputBuffer_.push(ev);
            observer_.notify(ev);
        } else if (!isDown && wasDown) {
            InputEvent ev{DeviceType::Gamepad, index, EventType::Release, b, 0.0f, 0, Clock::now()};
            inputBuffer_.push(ev);
            observer_.notify(ev);
        }
    }
}

void GamepadDevice::injectAxis(uint8_t axis, float value, DeviceIndex index) {
    if (index >= kMaxDevices || axis >= kMaxAxes) return;
    devices_[index].buffer.writableRef().axes[axis] = value;
    InputEvent ev{DeviceType::Gamepad, index, EventType::Axis, axis, value, 0, Clock::now()};
    observer_.notify(ev);
}

void GamepadDevice::setBatteryLevel(float level, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].batteryLevel = level;
}

void GamepadDevice::setWireless(bool wireless, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].wireless = wireless;
}

void GamepadDevice::setConnected(bool connected, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].connected = connected;
}

VibrationParams GamepadDevice::lastVibration(DeviceIndex index) const {
    if (index >= kMaxDevices) return {};
    return devices_[index].vibration;
}

float GamepadDevice::applyDeadZone(float value, float dz) const {
    if (std::abs(value) < dz) return 0.0f;
    float sign = value > 0.0f ? 1.0f : -1.0f;
    return sign * (std::abs(value) - dz) / (1.0f - dz);
}

} // namespace ergo::input
