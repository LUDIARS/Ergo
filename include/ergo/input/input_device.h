#pragma once

#include "ergo/input/types.h"

namespace ergo::input {

class IInputDevice {
public:
    virtual ~IInputDevice() = default;

    virtual void initialize() = 0;
    virtual void shutdown() = 0;

    virtual void poll() = 0;
    virtual void swapBuffers() = 0;

    virtual bool isConnected(DeviceIndex index = 0) const = 0;
    virtual DeviceType deviceType() const = 0;
};

} // namespace ergo::input
