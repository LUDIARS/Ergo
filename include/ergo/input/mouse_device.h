#pragma once

#include "ergo/input/input_device.h"
#include "ergo/input/double_buffer.h"
#include "ergo/input/input_buffer.h"
#include "ergo/input/observer.h"
#include <bitset>

namespace ergo::input {

class MouseDevice : public IInputDevice {
public:
    MouseDevice();
    ~MouseDevice() override;

    void initialize() override;
    void shutdown() override;
    void poll() override;
    void swapBuffers() override;
    bool isConnected(DeviceIndex index = 0) const override;
    DeviceType deviceType() const override { return DeviceType::Mouse; }

    bool isButtonDown(MouseButton button, DeviceIndex index = 0) const;
    bool isButtonPressed(MouseButton button, DeviceIndex index = 0) const;
    bool isButtonReleased(MouseButton button, DeviceIndex index = 0) const;

    Vec2f cursorPosition(DeviceIndex index = 0) const;
    Vec2f moveDelta(DeviceIndex index = 0) const;
    Vec2f scrollDelta(DeviceIndex index = 0) const;

    void setCursorLock(bool locked, DeviceIndex index = 0);
    bool isCursorLocked(DeviceIndex index = 0) const;

    Observer& observer();
    InputBuffer& inputBuffer();

    // For testing/injection: inject raw state
    void injectButtonState(uint8_t buttons, DeviceIndex index = 0);
    void injectPosition(Vec2f pos, DeviceIndex index = 0);
    void injectScroll(Vec2f scroll, DeviceIndex index = 0);
    void setConnected(bool connected, DeviceIndex index = 0);

    static constexpr DeviceIndex kMaxDevices = 4;

private:
    struct State {
        uint8_t buttons = 0;
        Vec2f position{};
        Vec2f delta{};
        Vec2f scroll{};
    };

    struct DeviceData {
        DoubleBuffer<State> buffer;
        State prevState;
        bool connected = false;
        bool cursorLocked = false;
        Vec2f lockedPosition{};
    };

    DeviceData devices_[kMaxDevices];
    Observer observer_;
    InputBuffer inputBuffer_;
    bool initialized_ = false;
};

} // namespace ergo::input
