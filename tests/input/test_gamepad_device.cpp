#include "gtest/gtest.h"
#include "ergo/input/gamepad_device.h"
#include <cmath>

using namespace ergo::input;

class GamepadDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        gp.initialize();
        gp.setConnected(true, 0);
    }
    GamepadDevice gp;
};

TEST_F(GamepadDeviceTest, ButtonPressRelease) {
    gp.injectButtonState(1 << static_cast<uint16_t>(GamepadButton::A));
    gp.swapBuffers();

    EXPECT_TRUE(gp.isButtonDown(GamepadButton::A));
    EXPECT_TRUE(gp.isButtonPressed(GamepadButton::A));

    gp.swapBuffers();
    EXPECT_TRUE(gp.isButtonDown(GamepadButton::A));
    EXPECT_FALSE(gp.isButtonPressed(GamepadButton::A));

    gp.injectButtonState(0);
    gp.swapBuffers();
    EXPECT_FALSE(gp.isButtonDown(GamepadButton::A));
    EXPECT_TRUE(gp.isButtonReleased(GamepadButton::A));
}

TEST_F(GamepadDeviceTest, StickDeadZoneZero) {
    gp.setDeadZone(0.2f);
    gp.injectAxis(0, 0.1f);  // Within dead zone
    gp.swapBuffers();
    EXPECT_FLOAT_EQ(gp.axisValue(0), 0.0f);
}

TEST_F(GamepadDeviceTest, StickDeadZoneNormalized) {
    gp.setDeadZone(0.2f);
    gp.injectAxis(0, 0.6f);  // Outside dead zone
    gp.swapBuffers();
    float expected = (0.6f - 0.2f) / (1.0f - 0.2f);
    EXPECT_NEAR(gp.axisValue(0), expected, 0.001f);
}

TEST_F(GamepadDeviceTest, TriggerRange) {
    gp.injectAxis(4, 0.5f);
    gp.injectAxis(5, 1.0f);
    gp.swapBuffers();

    EXPECT_NEAR(gp.leftTrigger(), 0.5f, 0.001f);
    EXPECT_NEAR(gp.rightTrigger(), 1.0f, 0.001f);
}

TEST_F(GamepadDeviceTest, VibrationParamsSentAndRetrieved) {
    VibrationParams params{0.5f, 0.8f};
    gp.setVibration(params);

    auto last = gp.lastVibration();
    EXPECT_FLOAT_EQ(last.lowFrequency, 0.5f);
    EXPECT_FLOAT_EQ(last.highFrequency, 0.8f);
}

TEST_F(GamepadDeviceTest, BatteryLevel) {
    gp.setBatteryLevel(0.75f);
    EXPECT_FLOAT_EQ(gp.batteryLevel(), 0.75f);
}

TEST_F(GamepadDeviceTest, MultipleDevices) {
    gp.setConnected(true, 1);
    gp.injectAxis(0, 0.5f, 0);
    gp.injectAxis(0, -0.3f, 1);
    gp.swapBuffers();

    EXPECT_NE(gp.axisValue(0, 0), gp.axisValue(0, 1));
}

TEST_F(GamepadDeviceTest, DisconnectedDeviceSafeFail) {
    EXPECT_FALSE(gp.isConnected(99));
    EXPECT_FALSE(gp.isButtonDown(GamepadButton::A, 99));
    EXPECT_FLOAT_EQ(gp.axisValue(0, 99), 0.0f);
}

TEST_F(GamepadDeviceTest, ObserverReceivesEvent) {
    InputEvent received{};
    DeviceIndex receivedIndex = 99;

    gp.observer().subscribe([&](const InputEvent& ev) {
        received = ev;
        receivedIndex = ev.deviceIndex;
    });

    gp.injectButtonState(1 << static_cast<uint16_t>(GamepadButton::B));

    EXPECT_EQ(received.deviceType, DeviceType::Gamepad);
    EXPECT_EQ(received.eventType, EventType::Press);
    EXPECT_EQ(receivedIndex, 0u);
}
