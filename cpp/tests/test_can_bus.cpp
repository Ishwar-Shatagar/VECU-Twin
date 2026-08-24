#include <gtest/gtest.h>
#include "CANBus.hpp"
#include <chrono>
#include <thread>
#include <atomic>

using namespace vecu;

TEST(CANBusTest, StartAndStop) {
    CANBus bus("TestBus");
    EXPECT_NO_THROW(bus.start());
    EXPECT_NO_THROW(bus.stop());
}

TEST(CANBusTest, SubscriberReceivesPublishedFrame) {
    CANBus bus("TestBus");
    bus.start();

    std::atomic<int> received{0};
    std::string received_msg;

    bus.subscribe("test_sub", [&](const CANFrame& f) {
        received++;
        received_msg = f.message_name;
    });

    std::array<uint8_t, 8> data{};
    CANFrame frame(0x101, 8, data, "ENGINE_ECU", "ENGINE_RPM");
    bus.publish(frame);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bus.stop();

    EXPECT_EQ(received.load(), 1);
    EXPECT_EQ(received_msg, "ENGINE_RPM");
}

TEST(CANBusTest, MultipleSubscribersAllReceive) {
    CANBus bus("TestBus");
    bus.start();

    std::atomic<int> sub1{0}, sub2{0}, sub3{0};
    bus.subscribe("s1", [&](const CANFrame&) { sub1++; });
    bus.subscribe("s2", [&](const CANFrame&) { sub2++; });
    bus.subscribe("s3", [&](const CANFrame&) { sub3++; });

    std::array<uint8_t, 8> data{};
    bus.publish(CANFrame(0x101, 8, data, "ECU1", "MSG"));

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    bus.stop();

    EXPECT_EQ(sub1.load(), 1);
    EXPECT_EQ(sub2.load(), 1);
    EXPECT_EQ(sub3.load(), 1);
}

TEST(CANBusTest, ArbitrationLowerIDDeliveredFirst) {
    CANBus bus("TestBus");
    bus.start();

    std::vector<uint32_t> delivery_order;
    std::mutex order_mutex;

    bus.subscribe("order_checker", [&](const CANFrame& f) {
        std::lock_guard<std::mutex> lock(order_mutex);
        delivery_order.push_back(f.can_id);
    });

    // Publish high-ID frame first, then low-ID frame
    std::array<uint8_t, 8> data{};
    bus.publish(CANFrame(0x500, 8, data, "ECU_HIGH", "MSG_HIGH"));
    bus.publish(CANFrame(0x100, 8, data, "ECU_LOW",  "MSG_LOW"));

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bus.stop();

    // Both should be received
    EXPECT_EQ(delivery_order.size(), 2u);
    // Due to batch arbitration, lower ID should be delivered before higher ID
    if (delivery_order.size() == 2) {
        EXPECT_LE(delivery_order[0], delivery_order[1]);
    }
}

TEST(CANBusTest, StatisticsCountCorrectly) {
    CANBus bus("TestBus");
    bus.start();

    std::atomic<int> received{0};
    bus.subscribe("counter", [&](const CANFrame&) { received++; });

    std::array<uint8_t, 8> data{};
    for (int i = 0; i < 10; ++i) {
        bus.publish(CANFrame(0x101, 8, data, "ECU", "MSG"));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bus.stop();

    EXPECT_EQ(bus.getTotalFramesPublished(), 10u);
    EXPECT_EQ(bus.getTotalFramesDelivered(), 10u);
    EXPECT_EQ(received.load(), 10);
}

TEST(CANBusTest, PublishBeforeStartIsNoOp) {
    CANBus bus("TestBus");
    std::atomic<int> received{0};
    bus.subscribe("s", [&](const CANFrame&) { received++; });

    std::array<uint8_t, 8> data{};
    // Bus not started — publish should be ignored
    bus.publish(CANFrame(0x101, 8, data, "ECU", "MSG"));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(received.load(), 0);
}

TEST(CANBusTest, HighThroughputNoCrash) {
    CANBus bus("TestBus");
    bus.start();

    std::atomic<int> received{0};
    bus.subscribe("counter", [&](const CANFrame&) { received++; });

    std::array<uint8_t, 8> data{};
    const int N = 500;
    for (int i = 0; i < N; ++i) {
        bus.publish(CANFrame(0x100 + (i % 10), 8, data, "ECU", "MSG"));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    bus.stop();

    EXPECT_EQ(received.load(), N);
}
