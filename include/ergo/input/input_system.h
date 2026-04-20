#pragma once

#include "ergo/input/types.h"
#include "ergo/input/mouse_device.h"
#include "ergo/input/keyboard_device.h"
#include "ergo/input/gamepad_device.h"
#include "ergo/input/usb_device.h"
#include "ergo/input/polling_thread.h"
#include <functional>
#include <vector>
#include <memory>

namespace ergo::input {

using DeviceConnectionCallback = std::function<void(DeviceType, DeviceIndex, bool /*connected*/)>;

class InputSystem {
public:
    InputSystem();
    ~InputSystem();

    void initialize(const InputConfig& config);
    void shutdown();

    void beginFrame();
    void endFrame();

    MouseDevice* mouse();
    KeyboardDevice* keyboard();
    GamepadDevice* gamepad();
    UsbDevice* usb();

    const MouseDevice* mouse() const;
    const KeyboardDevice* keyboard() const;
    const GamepadDevice* gamepad() const;
    const UsbDevice* usb() const;

    SubscriptionHandle onDeviceConnection(DeviceConnectionCallback callback);
    void removeDeviceConnectionCallback(SubscriptionHandle handle);

    void notifyDeviceConnected(DeviceType type, DeviceIndex index);
    void notifyDeviceDisconnected(DeviceType type, DeviceIndex index);

    const InputConfig& config() const;

private:
    InputConfig config_;
    std::unique_ptr<MouseDevice> mouse_;
    std::unique_ptr<KeyboardDevice> keyboard_;
    std::unique_ptr<GamepadDevice> gamepad_;
    std::unique_ptr<UsbDevice> usb_;
    std::unique_ptr<PollingThread> pollingThread_;

    struct ConnectionSub {
        SubscriptionHandle handle;
        DeviceConnectionCallback callback;
    };
    std::vector<ConnectionSub> connectionCallbacks_;
    SubscriptionHandle nextConnectionHandle_ = 1;
    bool initialized_ = false;
};

} // namespace ergo::input
