#include "gtest/gtest.h"
#include "ergo/input/polling_thread.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace ergo::input;

class MockDevice : public IInputDevice {
public:
    void initialize() override {}
    void shutdown() override {}
    void poll() override { pollCount.fetch_add(1); }
    void swapBuffers() override {}
    bool isConnected(DeviceIndex) const override { return connected; }
    DeviceType deviceType() const override { return DeviceType::Mouse; }

    std::atomic<int> pollCount{0};
    bool connected = true;
};

TEST(PollingThread, StartRunsThread) {
    MockDevice dev;
    PollingThread pt;

    pt.start(&dev, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_TRUE(pt.isRunning());
    EXPECT_GT(dev.pollCount.load(), 0);

    pt.stop();
}

TEST(PollingThread, StopStopsThread) {
    MockDevice dev;
    PollingThread pt;

    pt.start(&dev, 500);
    pt.stop();

    EXPECT_FALSE(pt.isRunning());
    int countAfterStop = dev.pollCount.load();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_EQ(dev.pollCount.load(), countAfterStop);
}

TEST(PollingThread, IntervalCanBeChanged) {
    MockDevice dev;
    PollingThread pt;

    pt.start(&dev, 100000); // 100ms interval
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int count1 = dev.pollCount.load();

    pt.setInterval(500); // 0.5ms interval - much faster
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    int count2 = dev.pollCount.load();

    EXPECT_GT(count2 - count1, 0);
    pt.stop();
}

TEST(PollingThread, DoubleStartIsSafe) {
    MockDevice dev;
    PollingThread pt;

    pt.start(&dev, 500);
    pt.start(&dev, 500); // Should not crash or create a second thread
    EXPECT_TRUE(pt.isRunning());

    pt.stop();
}

TEST(PollingThread, DisconnectedDeviceNoCrash) {
    MockDevice dev;
    dev.connected = false;

    PollingThread pt;
    pt.start(&dev, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Should not crash even though device is disconnected
    pt.stop();
    EXPECT_FALSE(pt.isRunning());
}
