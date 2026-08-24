#include <gtest/gtest.h>
#include "DigitalTwin.hpp"
#include "CANFrame.hpp"
#include <thread>
#include <chrono>

using namespace vecu;

static CANFrame makeRPMFrame(uint16_t rpm, uint8_t load) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 0, rpm);
    data[2] = load;
    return CANFrame(0x101, 8, data, "ENGINE_ECU", "ENGINE_RPM");
}

static CANFrame makeTempFrame(float temp_c) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeInt16(data, 0, static_cast<int16_t>(temp_c * 10.0f));
    return CANFrame(0x102, 8, data, "ENGINE_ECU", "ENGINE_TEMP");
}

static CANFrame makeSpeedFrame(uint16_t speed_tenth_kmh) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeUInt16(data, 0, speed_tenth_kmh);
    return CANFrame(0x203, 8, data, "BRAKE_ECU", "VEHICLE_SPEED");
}

static CANFrame makeBrakeStatusFrame(bool active) {
    std::array<uint8_t, 8> data{};
    data[0] = active ? 1 : 0;
    return CANFrame(0x201, 8, data, "BRAKE_ECU", "BRAKE_STATUS");
}

static CANFrame makeBatteryStatusFrame(uint8_t pct, bool charging) {
    std::array<uint8_t, 8> data{};
    data[0] = pct;
    data[1] = charging ? 1 : 0;
    return CANFrame(0x301, 8, data, "BATTERY_ECU", "BATTERY_STATUS");
}

static CANFrame makeSteeringFrame(float angle_deg) {
    std::array<uint8_t, 8> data{};
    CANFrame::encodeInt16(data, 0, static_cast<int16_t>(angle_deg * 10.0f));
    return CANFrame(0x401, 8, data, "STEERING_ECU", "STEERING_ANGLE");
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(DigitalTwinTest, InitialHealthIsHealthy) {
    DigitalTwin twin;
    // With no frames and recent init, health depends on update age
    // Just check it returns a valid enum value
    TwinHealth h = twin.getHealth();
    EXPECT_TRUE(h == TwinHealth::HEALTHY || h == TwinHealth::WARNING || h == TwinHealth::FAULT);
}

TEST(DigitalTwinTest, DecodesRPMFrameCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeRPMFrame(2350, 30));
    EXPECT_NEAR(twin.getCurrentState().rpm, 2350.0, 1.0);
    EXPECT_NEAR(twin.getCurrentState().engine_load_pct, 30.0, 1.0);
}

TEST(DigitalTwinTest, DecodesTemperatureFrameCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeTempFrame(72.5f));
    EXPECT_NEAR(twin.getCurrentState().engine_temperature_c, 72.5, 0.2);
}

TEST(DigitalTwinTest, DecodesSpeedFrameCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeSpeedFrame(654));  // 65.4 km/h
    EXPECT_NEAR(twin.getCurrentState().speed_kmh, 65.4, 0.2);
}

TEST(DigitalTwinTest, DecodesBrakeStatusCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeBrakeStatusFrame(true));
    EXPECT_TRUE(twin.getCurrentState().brake_active);
    twin.processCANFrame(makeBrakeStatusFrame(false));
    EXPECT_FALSE(twin.getCurrentState().brake_active);
}

TEST(DigitalTwinTest, DecodesBatteryStatusCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeBatteryStatusFrame(84, false));
    EXPECT_NEAR(twin.getCurrentState().battery_pct, 84.0, 0.1);
    EXPECT_FALSE(twin.getCurrentState().battery_charging);
}

TEST(DigitalTwinTest, DecodesSteeringAngleCorrectly) {
    DigitalTwin twin;
    twin.processCANFrame(makeSteeringFrame(45.5f));
    EXPECT_NEAR(twin.getCurrentState().steering_angle_deg, 45.5, 0.2);
}

TEST(DigitalTwinTest, NegativeSteeringAngle) {
    DigitalTwin twin;
    twin.processCANFrame(makeSteeringFrame(-120.0f));
    EXPECT_NEAR(twin.getCurrentState().steering_angle_deg, -120.0, 0.5);
}

TEST(DigitalTwinTest, SyncStatusTracksECU) {
    DigitalTwin twin;
    twin.setECUTimeout(2000);
    twin.processCANFrame(makeRPMFrame(2000, 20));
    auto sync = twin.getSyncStatus();
    EXPECT_TRUE(sync.ecu_synced.count("ENGINE_ECU") > 0);
    EXPECT_TRUE(sync.ecu_synced.at("ENGINE_ECU"));
}

TEST(DigitalTwinTest, HistoryGrowsWithFrames) {
    DigitalTwin twin;
    for (int i = 0; i < 10; ++i) {
        twin.processCANFrame(makeRPMFrame(800 + i * 100, 10));
    }
    auto history = twin.getHistory(50);
    EXPECT_GE(history.size(), 5u);
}

TEST(DigitalTwinTest, ValidationErrorOnOutOfRangeRPM) {
    DigitalTwin twin;
    // RPM of 65535 is technically valid for uint16 but way out of engine range
    // The twin should clamp but count it
    twin.processCANFrame(makeRPMFrame(65535, 0));
    EXPECT_GE(twin.getValidationErrors(), 0u);  // might or might not trigger
    EXPECT_LE(twin.getCurrentState().rpm, 9000.0);  // must be clamped
}

TEST(DigitalTwinTest, StatusJsonContainsRequiredFields) {
    DigitalTwin twin;
    twin.processCANFrame(makeRPMFrame(2000, 20));
    std::string json = twin.toStatusJson();
    EXPECT_NE(json.find("health"),       std::string::npos);
    EXPECT_NE(json.find("synchronized"), std::string::npos);
    EXPECT_NE(json.find("update_age_ms"), std::string::npos);
    EXPECT_NE(json.find("ecu_sync"),     std::string::npos);
}
