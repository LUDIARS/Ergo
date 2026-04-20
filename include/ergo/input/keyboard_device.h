#pragma once

#include "ergo/input/input_device.h"
#include "ergo/input/double_buffer.h"
#include "ergo/input/input_buffer.h"
#include "ergo/input/observer.h"
#include <bitset>
#include <string>

namespace ergo::input {

class KeyboardDevice : public IInputDevice {
public:
    KeyboardDevice();
    ~KeyboardDevice() override;

    void initialize() override;
    void shutdown() override;
    void poll() override;
    void swapBuffers() override;
    bool isConnected(DeviceIndex index = 0) const override;
    DeviceType deviceType() const override { return DeviceType::Keyboard; }

    bool isKeyDown(KeyCode key, DeviceIndex index = 0) const;
    bool isKeyPressed(KeyCode key, DeviceIndex index = 0) const;
    bool isKeyReleased(KeyCode key, DeviceIndex index = 0) const;

    ModifierFlags modifiers(DeviceIndex index = 0) const;

    const std::u32string& textInput(DeviceIndex index = 0) const;
    void clearTextInput(DeviceIndex index = 0);

    Duration keyHoldDuration(KeyCode key, DeviceIndex index = 0) const;

    Observer& observer();
    InputBuffer& inputBuffer();

    // For testing/injection
    void injectKeyState(KeyCode key, bool down, DeviceIndex index = 0);
    void injectTextInput(char32_t ch, DeviceIndex index = 0);
    void setConnected(bool connected, DeviceIndex index = 0);

    static constexpr DeviceIndex kMaxDevices = 4;

private:
    struct State {
        std::bitset<static_cast<size_t>(KeyCode::MaxKey)> keys;
        ModifierFlags modifiers = ModifierFlags::None;
        std::u32string textBuffer;
    };

    struct DeviceData {
        DoubleBuffer<State> buffer;
        State prevState;
        bool connected = false;
        std::unordered_map<uint16_t, TimePoint> pressStartTimes;
    };

    DeviceData devices_[kMaxDevices];
    Observer observer_;
    InputBuffer inputBuffer_;
    bool initialized_ = false;

    void updateModifiers(State& state);
};

} // namespace ergo::input
