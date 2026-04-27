#include "ergo/input/keyboard_device.h"

#include "ergo/log/log.h"

namespace ergo::input {

namespace {

/// KeyCode → 短い人間可読ラベル。デバッグログ専用。Unknown キーは
/// `code=NN` の数値表記にフォールバック (新規 KeyCode を増やしても
/// ここを忘れて compile エラーにならないように、`switch` ではなく
/// テーブル探索)。
const char* keycode_label(KeyCode key) {
    using K = KeyCode;
    switch (key) {
        case K::A: return "A"; case K::B: return "B"; case K::C: return "C";
        case K::D: return "D"; case K::E: return "E"; case K::F: return "F";
        case K::G: return "G"; case K::H: return "H"; case K::I: return "I";
        case K::J: return "J"; case K::K: return "K"; case K::L: return "L";
        case K::M: return "M"; case K::N: return "N"; case K::O: return "O";
        case K::P: return "P"; case K::Q: return "Q"; case K::R: return "R";
        case K::S: return "S"; case K::T: return "T"; case K::U: return "U";
        case K::V: return "V"; case K::W: return "W"; case K::X: return "X";
        case K::Y: return "Y"; case K::Z: return "Z";
        case K::Num1: return "1"; case K::Num2: return "2"; case K::Num3: return "3";
        case K::Num4: return "4"; case K::Num5: return "5"; case K::Num6: return "6";
        case K::Num7: return "7"; case K::Num8: return "8"; case K::Num9: return "9";
        case K::Num0: return "0";
        case K::Enter:     return "Enter";
        case K::Escape:    return "Escape";
        case K::Backspace: return "Backspace";
        case K::Tab:       return "Tab";
        case K::Space:     return "Space";
        case K::F1:  return "F1";  case K::F2:  return "F2";  case K::F3:  return "F3";
        case K::F4:  return "F4";  case K::F5:  return "F5";  case K::F6:  return "F6";
        case K::F7:  return "F7";  case K::F8:  return "F8";  case K::F9:  return "F9";
        case K::F10: return "F10"; case K::F11: return "F11"; case K::F12: return "F12";
        case K::Right: return "Right"; case K::Left: return "Left";
        case K::Down:  return "Down";  case K::Up:   return "Up";
        case K::LCtrl:  return "LCtrl";  case K::LShift: return "LShift";
        case K::LAlt:   return "LAlt";   case K::LSuper: return "LSuper";
        case K::RCtrl:  return "RCtrl";  case K::RShift: return "RShift";
        case K::RAlt:   return "RAlt";   case K::RSuper: return "RSuper";
        default: return nullptr;
    }
}

} // namespace

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
        if (const char* name = keycode_label(key)) {
            ERGO_LOG_INFO("[input] key down: %s (code=%u, dev=%u)", name, code, index);
        } else {
            ERGO_LOG_INFO("[input] key down: code=%u (dev=%u)", code, index);
        }
    } else if (!down && wasDown) {
        devices_[index].pressStartTimes.erase(code);
        InputEvent ev{DeviceType::Keyboard, index, EventType::Release, code, 0.0f, 0, now};
        inputBuffer_.push(ev);
        observer_.notify(ev);
        if (const char* name = keycode_label(key)) {
            ERGO_LOG_INFO("[input] key up:   %s (code=%u, dev=%u)", name, code, index);
        } else {
            ERGO_LOG_INFO("[input] key up:   code=%u (dev=%u)", code, index);
        }
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
