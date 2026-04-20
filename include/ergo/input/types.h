#pragma once

#include <cstdint>
#include <chrono>
#include <functional>

namespace ergo::input {

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;
using DeviceIndex = uint32_t;

enum class DeviceType : uint8_t {
    Mouse,
    Keyboard,
    Gamepad,
    UsbGeneric,
};

enum class ThreadMode : uint8_t {
    Independent,
    MainSync,
};

enum class DeliveryPolicy : uint8_t {
    Immediate,
    FrameSync,
};

enum class EventType : uint8_t {
    Press,
    Release,
    Repeat,
    Move,
    Scroll,
    Axis,
    Text,
    Connect,
    Disconnect,
};

enum class MouseButton : uint8_t {
    Left     = 0,
    Right    = 1,
    Middle   = 2,
    Button4  = 3,
    Button5  = 4,
    Count    = 5,
};

enum class KeyCode : uint16_t {
    Unknown = 0,
    A = 4, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num1 = 30, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9, Num0,
    Enter = 40, Escape, Backspace, Tab, Space,
    F1 = 58, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Right = 79, Left, Down, Up,
    LCtrl = 224, LShift, LAlt, LSuper,
    RCtrl, RShift, RAlt, RSuper,
    MaxKey = 512,
};

enum class ModifierFlags : uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Ctrl  = 1 << 1,
    Alt   = 1 << 2,
    Super = 1 << 3,
};

inline ModifierFlags operator|(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline ModifierFlags operator&(ModifierFlags a, ModifierFlags b) {
    return static_cast<ModifierFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool hasFlag(ModifierFlags flags, ModifierFlags test) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(test)) != 0;
}

enum class GamepadButton : uint16_t {
    A = 0, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DpadUp, DpadRight, DpadDown, DpadLeft,
    Count,
};

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct InputConfig {
    ThreadMode threadMode = ThreadMode::MainSync;
    uint64_t pollingIntervalUs = 1000;
    uint32_t bufferCapacity = 1024;
    bool enableMouse = true;
    bool enableKeyboard = true;
    bool enableGamepad = true;
    bool enableUsb = false;
};

struct InputEvent {
    DeviceType deviceType;
    DeviceIndex deviceIndex = 0;
    EventType eventType;
    uint16_t code = 0;       // KeyCode, MouseButton, GamepadButton, or axis index
    float value = 0.0f;
    uint64_t sequence = 0;
    TimePoint timestamp;
};

struct TimedInputEntry {
    InputEvent event;
    Duration holdDuration{};
};

struct VibrationParams {
    float lowFrequency = 0.0f;
    float highFrequency = 0.0f;
};

using SubscriptionHandle = uint64_t;
constexpr SubscriptionHandle InvalidHandle = 0;

} // namespace ergo::input
