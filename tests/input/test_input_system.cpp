#include "gtest/gtest.h"
#include "ergo/input/input_system.h"

using namespace ergo::input;

TEST(InputSystem, InitializesOnlyEnabledDevices) {
    InputSystem sys;
    InputConfig config;
    config.enableMouse = true;
    config.enableKeyboard = true;
    config.enableGamepad = false;
    config.enableUsb = false;
    sys.initialize(config);

    EXPECT_NE(sys.mouse(), nullptr);
    EXPECT_NE(sys.keyboard(), nullptr);
    EXPECT_EQ(sys.gamepad(), nullptr);
    EXPECT_EQ(sys.usb(), nullptr);
}

TEST(InputSystem, BeginFrameSwapsBuffers) {
    InputSystem sys;
    InputConfig config;
    config.enableMouse = true;
    config.enableKeyboard = false;
    config.enableGamepad = false;
    config.enableUsb = false;
    sys.initialize(config);

    sys.mouse()->setConnected(true);
    sys.mouse()->injectPosition({50.0f, 60.0f});
    sys.beginFrame();

    EXPECT_FLOAT_EQ(sys.mouse()->cursorPosition().x, 50.0f);
}

TEST(InputSystem, DeviceConnectCallback) {
    InputSystem sys;
    InputConfig config;
    sys.initialize(config);

    bool called = false;
    DeviceType receivedType{};
    DeviceIndex receivedIndex = 99;
    bool receivedConnected = false;

    sys.onDeviceConnection([&](DeviceType type, DeviceIndex idx, bool connected) {
        called = true;
        receivedType = type;
        receivedIndex = idx;
        receivedConnected = connected;
    });

    sys.notifyDeviceConnected(DeviceType::Gamepad, 1);

    EXPECT_TRUE(called);
    EXPECT_EQ(receivedType, DeviceType::Gamepad);
    EXPECT_EQ(receivedIndex, 1u);
    EXPECT_TRUE(receivedConnected);
}

TEST(InputSystem, DeviceDisconnectCallback) {
    InputSystem sys;
    InputConfig config;
    sys.initialize(config);

    bool disconnected = false;
    sys.onDeviceConnection([&](DeviceType, DeviceIndex, bool connected) {
        disconnected = !connected;
    });

    sys.notifyDeviceDisconnected(DeviceType::Mouse, 0);
    EXPECT_TRUE(disconnected);
}

TEST(InputSystem, UninitializedDeviceAccessSafeFail) {
    InputSystem sys;
    InputConfig config;
    config.enableMouse = false;
    config.enableKeyboard = false;
    config.enableGamepad = false;
    config.enableUsb = false;
    sys.initialize(config);

    EXPECT_EQ(sys.mouse(), nullptr);
    EXPECT_EQ(sys.keyboard(), nullptr);
    EXPECT_EQ(sys.gamepad(), nullptr);
    EXPECT_EQ(sys.usb(), nullptr);
}

TEST(InputSystem, ShutdownCleansUp) {
    InputSystem sys;
    InputConfig config;
    config.enableMouse = true;
    config.enableKeyboard = true;
    sys.initialize(config);
    EXPECT_NE(sys.mouse(), nullptr);

    sys.shutdown();
    EXPECT_EQ(sys.mouse(), nullptr);
    EXPECT_EQ(sys.keyboard(), nullptr);
}
