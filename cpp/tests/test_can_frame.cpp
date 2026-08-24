#include <gtest/gtest.h>
#include "CANFrame.hpp"

using namespace vecu;

TEST(CANFrameTest, ConstructionSetsFields) {
    std::array<uint8_t, 8> data{1, 2, 3, 4, 5, 6, 7, 8};
    CANFrame frame(0x101, 8, data, "ENGINE_ECU", "ENGINE_RPM");

    EXPECT_EQ(frame.can_id,       0x101u);
    EXPECT_EQ(frame.dlc,          8);
    EXPECT_EQ(frame.data,         data);
    EXPECT_EQ(frame.source_ecu,   "ENGINE_ECU");
    EXPECT_EQ(frame.message_name, "ENGINE_RPM");
    EXPECT_GT(frame.timestamp,    0.0);
}

TEST(CANFrameTest, DefaultConstructionIsValid) {
    CANFrame frame;
    EXPECT_EQ(frame.can_id, 0u);
    EXPECT_EQ(frame.dlc,    0);
}

TEST(CANFrameTest, EncodeDecodeUInt16RoundTrip) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 0, 2350);
    uint16_t result = CANFrame::decodeUInt16(data, 0);
    EXPECT_EQ(result, 2350);
}

TEST(CANFrameTest, EncodeDecodeUInt16MaxValue) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 0, 65535);
    EXPECT_EQ(CANFrame::decodeUInt16(data, 0), 65535);
}

TEST(CANFrameTest, EncodeDecodeUInt16ZeroValue) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 0, 0);
    EXPECT_EQ(CANFrame::decodeUInt16(data, 0), 0);
}

TEST(CANFrameTest, EncodeDecodeInt16PositiveValue) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeInt16(data, 0, 725);  // 72.5°C
    EXPECT_EQ(CANFrame::decodeInt16(data, 0), 725);
}

TEST(CANFrameTest, EncodeDecodeInt16NegativeValue) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeInt16(data, 0, -400);  // -40.0°C
    EXPECT_EQ(CANFrame::decodeInt16(data, 0), -400);
}

TEST(CANFrameTest, EncodeAtOffset2) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 2, 1234);
    // Bytes 0,1 should be untouched
    EXPECT_EQ(data[0], 0);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(CANFrame::decodeUInt16(data, 2), 1234);
}

TEST(CANFrameTest, BigEndianEncoding) {
    std::array<uint8_t, 8> data{};
    // 0x0917 = 2327, big-endian: byte[0]=0x09, byte[1]=0x17
    CANFrame::encodeUInt16(data, 0, 0x0917);
    EXPECT_EQ(data[0], 0x09);
    EXPECT_EQ(data[1], 0x17);
}

TEST(CANFrameTest, PriorityLowerIDWins) {
    std::array<uint8_t, 8> d{};
    CANFrame high_priority(0x101, 8, d, "ENGINE_ECU", "ENGINE_RPM");
    CANFrame low_priority (0x201, 8, d, "BRAKE_ECU",  "BRAKE_STATUS");
    EXPECT_TRUE(high_priority.hasHigherPriorityThan(low_priority));
    EXPECT_FALSE(low_priority.hasHigherPriorityThan(high_priority));
}

TEST(CANFrameTest, EqualIDsNoHigherPriority) {
    std::array<uint8_t, 8> d{};
    CANFrame a(0x101, 8, d, "ECU1", "MSG");
    CANFrame b(0x101, 8, d, "ECU1", "MSG");
    EXPECT_FALSE(a.hasHigherPriorityThan(b));
}

TEST(CANFrameTest, ToJsonContainsRequiredFields) {
    std::array<uint8_t, 8> data{65, 0, 120, 9, 0, 0, 0, 0};
    CANFrame frame(0x101, 8, data, "ENGINE_ECU", "ENGINE_RPM");
    std::string json = frame.toJson();
    EXPECT_NE(json.find("timestamp"),    std::string::npos);
    EXPECT_NE(json.find("can_id"),       std::string::npos);
    EXPECT_NE(json.find("dlc"),          std::string::npos);
    EXPECT_NE(json.find("data"),         std::string::npos);
    EXPECT_NE(json.find("source"),       std::string::npos);
    EXPECT_NE(json.find("message"),      std::string::npos);
    EXPECT_NE(json.find("ENGINE_ECU"),   std::string::npos);
    EXPECT_NE(json.find("ENGINE_RPM"),   std::string::npos);
}

TEST(CANFrameTest, NowReturnsSensibleTimestamp) {
    double t1 = CANFrame::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    double t2 = CANFrame::now();
    EXPECT_GT(t2, t1);
    EXPECT_LT(t2 - t1, 1.0);  // less than 1 second
}
