#include "gtest/gtest.h"
#include "ergo/input/usb_device.h"

using namespace ergo::input;

class UsbDeviceTest : public ::testing::Test {
protected:
    void SetUp() override {
        usb.initialize();
        usb.setConnected(true, 0);
    }
    UsbDevice usb;
};

TEST_F(UsbDeviceTest, RawReport) {
    std::vector<uint8_t> report = {0x01, 0x02, 0xFF};
    usb.injectRawReport(report);
    usb.swapBuffers();

    auto& raw = usb.rawReport();
    ASSERT_EQ(raw.size(), 3u);
    EXPECT_EQ(raw[0], 0x01);
    EXPECT_EQ(raw[2], 0xFF);
}

TEST_F(UsbDeviceTest, DescriptorParsed) {
    HidDescriptor desc{0x1234, 0x5678, 3, 8};
    usb.injectDescriptor(desc);

    auto& d = usb.descriptor();
    EXPECT_EQ(d.vendorId, 0x1234);
    EXPECT_EQ(d.productId, 0x5678);
    EXPECT_EQ(d.axisCount, 3u);
    EXPECT_EQ(d.buttonCount, 8u);
}

TEST_F(UsbDeviceTest, AxisValue) {
    usb.injectAxis(0, 0.5f);
    usb.injectAxis(1, -0.3f);
    usb.swapBuffers();

    EXPECT_NEAR(usb.axisValue(0), 0.5f, 0.001f);
    EXPECT_NEAR(usb.axisValue(1), -0.3f, 0.001f);
}

TEST_F(UsbDeviceTest, ButtonState) {
    usb.injectButton(0, true);
    usb.injectButton(1, false);
    usb.swapBuffers();

    EXPECT_TRUE(usb.buttonState(0));
    EXPECT_FALSE(usb.buttonState(1));
}

TEST_F(UsbDeviceTest, MultipleDevices) {
    usb.setConnected(true, 1);
    usb.injectAxis(0, 0.1f, 0);
    usb.injectAxis(0, 0.9f, 1);
    usb.swapBuffers();

    EXPECT_NEAR(usb.axisValue(0, 0), 0.1f, 0.001f);
    EXPECT_NEAR(usb.axisValue(0, 1), 0.9f, 0.001f);
}

TEST_F(UsbDeviceTest, DisconnectedDeviceSafeFail) {
    EXPECT_FALSE(usb.isConnected(99));
    EXPECT_TRUE(usb.rawReport(99).empty());
    EXPECT_FLOAT_EQ(usb.axisValue(0, 99), 0.0f);
    EXPECT_FALSE(usb.buttonState(0, 99));
}

TEST_F(UsbDeviceTest, ObserverReceivesEvent) {
    InputEvent received{};
    DeviceIndex receivedIndex = 99;

    usb.observer().subscribe([&](const InputEvent& ev) {
        received = ev;
        receivedIndex = ev.deviceIndex;
    });

    usb.injectButton(0, true);

    EXPECT_EQ(received.deviceType, DeviceType::UsbGeneric);
    EXPECT_EQ(received.eventType, EventType::Press);
    EXPECT_EQ(receivedIndex, 0u);
}
