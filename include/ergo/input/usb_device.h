#pragma once

#include "ergo/input/input_device.h"
#include "ergo/input/double_buffer.h"
#include "ergo/input/input_buffer.h"
#include "ergo/input/observer.h"
#include <vector>
#include <unordered_map>

namespace ergo::input {

struct HidDescriptor {
    uint16_t vendorId = 0;
    uint16_t productId = 0;
    uint32_t axisCount = 0;
    uint32_t buttonCount = 0;
};

class UsbDevice : public IInputDevice {
public:
    UsbDevice();
    ~UsbDevice() override;

    void initialize() override;
    void shutdown() override;
    void poll() override;
    void swapBuffers() override;
    bool isConnected(DeviceIndex index = 0) const override;
    DeviceType deviceType() const override { return DeviceType::UsbGeneric; }

    const std::vector<uint8_t>& rawReport(DeviceIndex index = 0) const;
    float axisValue(uint32_t axis, DeviceIndex index = 0) const;
    bool buttonState(uint32_t button, DeviceIndex index = 0) const;
    const HidDescriptor& descriptor(DeviceIndex index = 0) const;

    Observer& observer();
    InputBuffer& inputBuffer();

    // For testing/injection
    void injectRawReport(const std::vector<uint8_t>& report, DeviceIndex index = 0);
    void injectDescriptor(const HidDescriptor& desc, DeviceIndex index = 0);
    void injectAxis(uint32_t axis, float value, DeviceIndex index = 0);
    void injectButton(uint32_t button, bool state, DeviceIndex index = 0);
    void setConnected(bool connected, DeviceIndex index = 0);

    static constexpr DeviceIndex kMaxDevices = 8;

private:
    struct State {
        std::vector<uint8_t> rawReport;
        std::unordered_map<uint32_t, float> axes;
        std::unordered_map<uint32_t, bool> buttons;
    };

    struct DeviceData {
        DoubleBuffer<State> buffer;
        bool connected = false;
        HidDescriptor descriptor;
    };

    DeviceData devices_[kMaxDevices];
    Observer observer_;
    InputBuffer inputBuffer_;
    bool initialized_ = false;
    static const std::vector<uint8_t> emptyReport_;
};

} // namespace ergo::input
