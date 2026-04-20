#pragma once

#include "ergo/input/input_device.h"
#include "ergo/input/double_buffer.h"
#include "ergo/input/input_buffer.h"
#include "ergo/input/observer.h"
#include <array>

namespace ergo::input {

class GamepadDevice : public IInputDevice {
public:
    GamepadDevice();
    ~GamepadDevice() override;

    void initialize() override;
    void shutdown() override;
    void poll() override;
    void swapBuffers() override;
    bool isConnected(DeviceIndex index = 0) const override;
    DeviceType deviceType() const override { return DeviceType::Gamepad; }

    bool isButtonDown(GamepadButton button, DeviceIndex index = 0) const;
    bool isButtonPressed(GamepadButton button, DeviceIndex index = 0) const;
    bool isButtonReleased(GamepadButton button, DeviceIndex index = 0) const;

    float axisValue(uint8_t axis, DeviceIndex index = 0) const;
    Vec2f leftStick(DeviceIndex index = 0) const;
    Vec2f rightStick(DeviceIndex index = 0) const;
    float leftTrigger(DeviceIndex index = 0) const;
    float rightTrigger(DeviceIndex index = 0) const;

    void setDeadZone(float deadZone, DeviceIndex index = 0);
    float deadZone(DeviceIndex index = 0) const;

    void setVibration(VibrationParams params, DeviceIndex index = 0);

    float batteryLevel(DeviceIndex index = 0) const;
    bool isWireless(DeviceIndex index = 0) const;

    Observer& observer();
    InputBuffer& inputBuffer();

    // For testing/injection
    void injectButtonState(uint32_t buttons, DeviceIndex index = 0);
    void injectAxis(uint8_t axis, float value, DeviceIndex index = 0);
    void setBatteryLevel(float level, DeviceIndex index = 0);
    void setWireless(bool wireless, DeviceIndex index = 0);
    void setConnected(bool connected, DeviceIndex index = 0);
    VibrationParams lastVibration(DeviceIndex index = 0) const;

    static constexpr DeviceIndex kMaxDevices = 8;
    static constexpr uint8_t kMaxAxes = 6;

private:
    struct State {
        uint32_t buttons = 0;
        std::array<float, kMaxAxes> axes{};
    };

    struct DeviceData {
        DoubleBuffer<State> buffer;
        State prevState;
        bool connected = false;
        bool wireless = false;
        float batteryLevel = 1.0f;
        float deadZone = 0.1f;
        VibrationParams vibration;
    };

    DeviceData devices_[kMaxDevices];
    Observer observer_;
    InputBuffer inputBuffer_;
    bool initialized_ = false;

    float applyDeadZone(float value, float dz) const;
};

} // namespace ergo::input
