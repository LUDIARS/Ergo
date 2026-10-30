#include "ergo/input/keyboard_device.h"

namespace ergo::input {

KeyboardDevice::KeyboardDevice() = default;
KeyboardDevice::~KeyboardDevice() { shutdown(); }

void KeyboardDevice::initialize() { initialized_ = true; }
void KeyboardDevice::shutdown() { initialized_ = false; }

void KeyboardDevice::poll() {
    // Platform-specific polling would go here.
}

void KeyboardDevice::swapBuffers() {
    for (DeviceIndex i = 0; i < kMaxDevices; ++i) {
        if (!devices_[i].connected) continue;
        auto& dev = devices_[i];
        dev.prevState = dev.buffer.read();
        dev.buffer.swap();
    }
}

bool KeyboardDevice::isConnected(DeviceIndex index) const {
    if (index >= kMaxDevices) return false;
    return devices_[index].connected;
}

bool KeyboardDevice::isKeyDown(KeyCode key, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    return devices_[index].buffer.read().keys.test(static_cast<size_t>(key));
}

bool KeyboardDevice::isKeyPressed(KeyCode key, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    size_t k = static_cast<size_t>(key);
    bool cur = devices_[index].buffer.read().keys.test(k);
    bool prev = devices_[index].prevState.keys.test(k);
    return cur && !prev;
}

bool KeyboardDevice::isKeyReleased(KeyCode key, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return false;
    size_t k = static_cast<size_t>(key);
    bool cur = devices_[index].buffer.read().keys.test(k);
    bool prev = devices_[index].prevState.keys.test(k);
    return !cur && prev;
}

ModifierFlags KeyboardDevice::modifiers(DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return ModifierFlags::None;
    return devices_[index].buffer.read().modifiers;
}

const std::u32string& KeyboardDevice::textInput(DeviceIndex index) const {
    static const std::u32string empty;
    if (index >= kMaxDevices || !devices_[index].connected) return empty;
    return devices_[index].buffer.read().textBuffer;
}

void KeyboardDevice::clearTextInput(DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().textBuffer.clear();
}

Duration KeyboardDevice::keyHoldDuration(KeyCode key, DeviceIndex index) const {
    if (index >= kMaxDevices || !devices_[index].connected) return Duration{};
    auto it = devices_[index].pressStartTimes.find(static_cast<uint16_t>(key));
    if (it != devices_[index].pressStartTimes.end()) {
        return Clock::now() - it->second;
    }
    return Duration{};
}

Observer& KeyboardDevice::observer() { return observer_; }
InputBuffer& KeyboardDevice::inputBuffer() { return inputBuffer_; }

void KeyboardDevice::injectKeyState(KeyCode key, bool down, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    auto& state = devices_[index].buffer.writableRef();
    size_t k = static_cast<size_t>(key);
    bool wasDown = state.keys.test(k);
    state.keys.set(k, down);
    updateModifiers(state);

    auto now = Clock::now();
    uint16_t code = static_cast<uint16_t>(key);

    if (down && !wasDown) {
        devices_[index].pressStartTimes[code] = now;
        InputEvent ev{DeviceType::Keyboard, index, EventType::Press, code, 1.0f, 0, now};
        inputBuffer_.push(ev);
        observer_.notify(ev);
    } else if (!down && wasDown) {
        devices_[index].pressStartTimes.erase(code);
        InputEvent ev{DeviceType::Keyboard, index, EventType::Release, code, 0.0f, 0, now};
        inputBuffer_.push(ev);
        observer_.notify(ev);
    }
}

void KeyboardDevice::injectTextInput(char32_t ch, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].buffer.writableRef().textBuffer.push_back(ch);
    InputEvent ev{DeviceType::Keyboard, index, EventType::Text, 0, 0.0f, 0, Clock::now()};
    observer_.notify(ev);
}

void KeyboardDevice::setConnected(bool connected, DeviceIndex index) {
    if (index >= kMaxDevices) return;
    devices_[index].connected = connected;
}

void KeyboardDevice::updateModifiers(State& state) {
    ModifierFlags mods = ModifierFlags::None;
    if (state.keys.test(static_cast<size_t>(KeyCode::LShift)) ||
        state.keys.test(static_cast<size_t>(KeyCode::RShift)))
        mods = mods | ModifierFlags::Shift;
    if (state.keys.test(static_cast<size_t>(KeyCode::LCtrl)) ||
        state.keys.test(static_cast<size_t>(KeyCode::RCtrl)))
        mods = mods | ModifierFlags::Ctrl;
    if (state.keys.test(static_cast<size_t>(KeyCode::LAlt)) ||
        state.keys.test(static_cast<size_t>(KeyCode::RAlt)))
        mods = mods | ModifierFlags::Alt;
    if (state.keys.test(static_cast<size_t>(KeyCode::LSuper)) ||
        state.keys.test(static_cast<size_t>(KeyCode::RSuper)))
        mods = mods | ModifierFlags::Super;
    state.modifiers = mods;
}

} // namespace ergo::input
