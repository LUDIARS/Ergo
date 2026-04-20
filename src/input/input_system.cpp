#include "ergo/input/input_system.h"

namespace ergo::input {

InputSystem::InputSystem() = default;

InputSystem::~InputSystem() {
    shutdown();
}

void InputSystem::initialize(const InputConfig& config) {
    if (initialized_) return;
    config_ = config;

    if (config.enableMouse) {
        mouse_ = std::make_unique<MouseDevice>();
        mouse_->initialize();
    }
    if (config.enableKeyboard) {
        keyboard_ = std::make_unique<KeyboardDevice>();
        keyboard_->initialize();
    }
    if (config.enableGamepad) {
        gamepad_ = std::make_unique<GamepadDevice>();
        gamepad_->initialize();
    }
    if (config.enableUsb) {
        usb_ = std::make_unique<UsbDevice>();
        usb_->initialize();
    }

    if (config.threadMode == ThreadMode::Independent) {
        pollingThread_ = std::make_unique<PollingThread>();
        // In a real implementation, the polling thread would poll all devices.
    }

    initialized_ = true;
}

void InputSystem::shutdown() {
    if (!initialized_) return;

    if (pollingThread_) {
        pollingThread_->stop();
        pollingThread_.reset();
    }

    if (mouse_) { mouse_->shutdown(); mouse_.reset(); }
    if (keyboard_) { keyboard_->shutdown(); keyboard_.reset(); }
    if (gamepad_) { gamepad_->shutdown(); gamepad_.reset(); }
    if (usb_) { usb_->shutdown(); usb_.reset(); }

    connectionCallbacks_.clear();
    initialized_ = false;
}

void InputSystem::beginFrame() {
    if (mouse_) {
        mouse_->poll();
        mouse_->swapBuffers();
    }
    if (keyboard_) {
        keyboard_->poll();
        keyboard_->swapBuffers();
    }
    if (gamepad_) {
        gamepad_->poll();
        gamepad_->swapBuffers();
    }
    if (usb_) {
        usb_->poll();
        usb_->swapBuffers();
    }
}

void InputSystem::endFrame() {
    // Reserved for end-of-frame cleanup.
}

MouseDevice* InputSystem::mouse() { return mouse_.get(); }
KeyboardDevice* InputSystem::keyboard() { return keyboard_.get(); }
GamepadDevice* InputSystem::gamepad() { return gamepad_.get(); }
UsbDevice* InputSystem::usb() { return usb_.get(); }

const MouseDevice* InputSystem::mouse() const { return mouse_.get(); }
const KeyboardDevice* InputSystem::keyboard() const { return keyboard_.get(); }
const GamepadDevice* InputSystem::gamepad() const { return gamepad_.get(); }
const UsbDevice* InputSystem::usb() const { return usb_.get(); }

SubscriptionHandle InputSystem::onDeviceConnection(DeviceConnectionCallback callback) {
    auto handle = nextConnectionHandle_++;
    connectionCallbacks_.push_back({handle, std::move(callback)});
    return handle;
}

void InputSystem::removeDeviceConnectionCallback(SubscriptionHandle handle) {
    connectionCallbacks_.erase(
        std::remove_if(connectionCallbacks_.begin(), connectionCallbacks_.end(),
                        [handle](const ConnectionSub& s) { return s.handle == handle; }),
        connectionCallbacks_.end());
}

void InputSystem::notifyDeviceConnected(DeviceType type, DeviceIndex index) {
    auto subs = connectionCallbacks_;
    for (const auto& sub : subs) {
        sub.callback(type, index, true);
    }
}

void InputSystem::notifyDeviceDisconnected(DeviceType type, DeviceIndex index) {
    auto subs = connectionCallbacks_;
    for (const auto& sub : subs) {
        sub.callback(type, index, false);
    }
}

const InputConfig& InputSystem::config() const {
    return config_;
}

} // namespace ergo::input
