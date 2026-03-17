#include "gtest/gtest.h"
#include "ergo/input/mouse_device.h"

using namespace ergo::input;

class MouseDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        mouse.initialize();
        mouse.setConnected(true, 0);
    }
    MouseDevice mouse;
};

TEST_F(MouseDeviceTest, ButtonPressReleaseTrigger) {
    mouse.injectButtonState(1 << static_cast<uint8_t>(MouseButton::Left));
    mouse.swapBuffers();

    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.isButtonPressed(MouseButton::Left));
    EXPECT_FALSE(mouse.isButtonReleased(MouseButton::Left));

    // Next frame, same state -> no longer "pressed" trigger
    mouse.swapBuffers();
    EXPECT_TRUE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_FALSE(mouse.isButtonPressed(MouseButton::Left));

    // Release
    mouse.injectButtonState(0);
    mouse.swapBuffers();
    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Left));
    EXPECT_TRUE(mouse.isButtonReleased(MouseButton::Left));
}

TEST_F(MouseDeviceTest, CursorPositionAndDelta) {
    mouse.injectPosition({100.0f, 200.0f});
    mouse.swapBuffers();
    EXPECT_FLOAT_EQ(mouse.cursorPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(mouse.cursorPosition().y, 200.0f);

    mouse.injectPosition({110.0f, 205.0f});
    mouse.swapBuffers();
    EXPECT_FLOAT_EQ(mouse.moveDelta().x, 10.0f);
    EXPECT_FLOAT_EQ(mouse.moveDelta().y, 5.0f);
}

TEST_F(MouseDeviceTest, ScrollDelta) {
    mouse.injectScroll({0.0f, 3.0f});
    mouse.swapBuffers();
    EXPECT_FLOAT_EQ(mouse.scrollDelta().y, 3.0f);
}

TEST_F(MouseDeviceTest, CursorLockFixesPosition) {
    mouse.injectPosition({100.0f, 200.0f});
    mouse.swapBuffers();
    mouse.setCursorLock(true);

    mouse.injectPosition({150.0f, 250.0f});
    mouse.swapBuffers();

    EXPECT_FLOAT_EQ(mouse.cursorPosition().x, 100.0f);
    EXPECT_FLOAT_EQ(mouse.cursorPosition().y, 200.0f);
    EXPECT_FLOAT_EQ(mouse.moveDelta().x, 50.0f);
    EXPECT_FLOAT_EQ(mouse.moveDelta().y, 50.0f);
}

TEST_F(MouseDeviceTest, MultipleDevices) {
    mouse.setConnected(true, 1);
    mouse.injectPosition({10.0f, 20.0f}, 0);
    mouse.injectPosition({30.0f, 40.0f}, 1);
    mouse.swapBuffers();

    EXPECT_FLOAT_EQ(mouse.cursorPosition(0).x, 10.0f);
    EXPECT_FLOAT_EQ(mouse.cursorPosition(1).x, 30.0f);
}

TEST_F(MouseDeviceTest, DisconnectedDeviceSafeFail) {
    EXPECT_FALSE(mouse.isConnected(99));
    EXPECT_FALSE(mouse.isButtonDown(MouseButton::Left, 99));
    auto pos = mouse.cursorPosition(99);
    EXPECT_FLOAT_EQ(pos.x, 0.0f);
    EXPECT_FLOAT_EQ(pos.y, 0.0f);
}

TEST_F(MouseDeviceTest, ObserverReceivesEvent) {
    InputEvent received{};
    DeviceIndex receivedIndex = 99;

    mouse.observer().subscribe([&](const InputEvent& ev) {
        received = ev;
        receivedIndex = ev.deviceIndex;
    });

    mouse.injectButtonState(1 << static_cast<uint8_t>(MouseButton::Left));

    EXPECT_EQ(received.deviceType, DeviceType::Mouse);
    EXPECT_EQ(received.eventType, EventType::Press);
    EXPECT_EQ(receivedIndex, 0u);
}
