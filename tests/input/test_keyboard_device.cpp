#include "gtest/gtest.h"
#include "ergo/input/keyboard_device.h"
#include <thread>

using namespace ergo::input;

class KeyboardDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        kb.initialize();
        kb.setConnected(true, 0);
    }
    KeyboardDevice kb;
};

TEST_F(KeyboardDeviceTest, KeyPressReleaseRepeat) {
    kb.injectKeyState(KeyCode::A, true);
    kb.swapBuffers();

    EXPECT_TRUE(kb.isKeyDown(KeyCode::A));
    EXPECT_TRUE(kb.isKeyPressed(KeyCode::A));
    EXPECT_FALSE(kb.isKeyReleased(KeyCode::A));

    // Next frame same state => held but not pressed trigger
    kb.swapBuffers();
    EXPECT_TRUE(kb.isKeyDown(KeyCode::A));
    EXPECT_FALSE(kb.isKeyPressed(KeyCode::A));

    // Release
    kb.injectKeyState(KeyCode::A, false);
    kb.swapBuffers();
    EXPECT_FALSE(kb.isKeyDown(KeyCode::A));
    EXPECT_TRUE(kb.isKeyReleased(KeyCode::A));
}

TEST_F(KeyboardDeviceTest, ModifierCombinations) {
    kb.injectKeyState(KeyCode::LShift, true);
    kb.injectKeyState(KeyCode::LCtrl, true);
    kb.swapBuffers();

    auto mods = kb.modifiers();
    EXPECT_TRUE(hasFlag(mods, ModifierFlags::Shift));
    EXPECT_TRUE(hasFlag(mods, ModifierFlags::Ctrl));
    EXPECT_FALSE(hasFlag(mods, ModifierFlags::Alt));
}

TEST_F(KeyboardDeviceTest, TextInputUnicode) {
    kb.injectTextInput(U'H');
    kb.injectTextInput(U'e');
    kb.injectTextInput(U'l');
    kb.injectTextInput(U'l');
    kb.injectTextInput(U'o');
    kb.injectTextInput(U'\u3042'); // Hiragana 'a'
    kb.swapBuffers();

    auto& text = kb.textInput();
    EXPECT_EQ(text.size(), 6u);
    EXPECT_EQ(text[0], U'H');
    EXPECT_EQ(text[5], U'\u3042');
}

TEST_F(KeyboardDeviceTest, KeyHoldDuration) {
    kb.injectKeyState(KeyCode::Space, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto dur = kb.keyHoldDuration(KeyCode::Space);
    EXPECT_GT(std::chrono::duration_cast<std::chrono::milliseconds>(dur).count(), 0);
}

TEST_F(KeyboardDeviceTest, MultipleDevices) {
    kb.setConnected(true, 1);
    kb.injectKeyState(KeyCode::A, true, 0);
    kb.injectKeyState(KeyCode::B, true, 1);
    kb.swapBuffers();

    EXPECT_TRUE(kb.isKeyDown(KeyCode::A, 0));
    EXPECT_FALSE(kb.isKeyDown(KeyCode::B, 0));
    EXPECT_TRUE(kb.isKeyDown(KeyCode::B, 1));
    EXPECT_FALSE(kb.isKeyDown(KeyCode::A, 1));
}

TEST_F(KeyboardDeviceTest, DisconnectedDeviceSafeFail) {
    EXPECT_FALSE(kb.isConnected(99));
    EXPECT_FALSE(kb.isKeyDown(KeyCode::A, 99));
    EXPECT_EQ(kb.modifiers(99), ModifierFlags::None);
}

TEST_F(KeyboardDeviceTest, ObserverReceivesEvent) {
    InputEvent received{};
    DeviceIndex receivedIndex = 99;

    kb.observer().subscribe([&](const InputEvent& ev) {
        received = ev;
        receivedIndex = ev.deviceIndex;
    });

    kb.injectKeyState(KeyCode::Enter, true);

    EXPECT_EQ(received.deviceType, DeviceType::Keyboard);
    EXPECT_EQ(received.eventType, EventType::Press);
    EXPECT_EQ(received.code, static_cast<uint16_t>(KeyCode::Enter));
    EXPECT_EQ(receivedIndex, 0u);
}
