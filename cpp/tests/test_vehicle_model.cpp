#include <gtest/gtest.h>
#include "VehicleModel.hpp"

using namespace vecu;

TEST(VehicleModelTest, InitialModeIsIdle) {
    VehicleModel model;
    EXPECT_EQ(model.getMode(), VehicleMode::IDLE);
}

TEST(VehicleModelTest, BrakingModeWhenBrakeActive) {
    VehicleModel model;
    model.updateSpeed(50.0);
    model.updateBrake(true, 30.0);
    EXPECT_EQ(model.getMode(), VehicleMode::BRAKING);
}

TEST(VehicleModelTest, DrivingModeAtSpeed) {
    VehicleModel model;
    model.updateSpeed(60.0);
    model.updateRPM(2000.0, 20.0);
    model.updateBrake(false, 0.0);
    EXPECT_EQ(model.getMode(), VehicleMode::DRIVING);
}

TEST(VehicleModelTest, AcceleratingModeAtHighLoad) {
    VehicleModel model;
    model.updateSpeed(40.0);
    model.updateRPM(4000.0, 70.0);
    model.updateBrake(false, 0.0);
    EXPECT_EQ(model.getMode(), VehicleMode::ACCELERATING);
}

TEST(VehicleModelTest, FaultModeOverridesOthers) {
    VehicleModel model;
    model.updateSpeed(60.0);
    model.updateBrake(false, 0.0);
    model.setFaultMode(true);
    EXPECT_EQ(model.getMode(), VehicleMode::FAULT);
}

TEST(VehicleModelTest, FaultModeClearable) {
    VehicleModel model;
    model.setFaultMode(true);
    EXPECT_EQ(model.getMode(), VehicleMode::FAULT);
    model.setFaultMode(false);
    EXPECT_NE(model.getMode(), VehicleMode::FAULT);
}

TEST(VehicleModelTest, SpeedClampedToRange) {
    VehicleModel model;
    model.updateSpeed(999.0);
    EXPECT_LE(model.getCurrentState().speed_kmh, 250.0);
    model.updateSpeed(-5.0);
    EXPECT_GE(model.getCurrentState().speed_kmh, 0.0);
}

TEST(VehicleModelTest, HistoryRecordsStates) {
    VehicleModel model;
    for (int i = 0; i < 10; ++i) {
        model.updateSpeed(static_cast<double>(i * 5));
    }
    auto history = model.getHistory(100);
    EXPECT_GE(history.size(), 5u);
}

TEST(VehicleModelTest, BatteryUpdatePreservesValues) {
    VehicleModel model;
    model.updateBattery(75.0, 395.0, 30.0, false);
    auto state = model.getCurrentState();
    EXPECT_NEAR(state.battery_pct,           75.0,  0.1);
    EXPECT_NEAR(state.battery_voltage_v,     395.0, 0.1);
    EXPECT_NEAR(state.battery_temperature_c, 30.0,  0.1);
    EXPECT_FALSE(state.battery_charging);
}

TEST(VehicleModelTest, SteeringUpdatePreservesAngle) {
    VehicleModel model;
    model.updateSteering(45.5);
    EXPECT_NEAR(model.getCurrentState().steering_angle_deg, 45.5, 0.1);
}

TEST(VehicleModelTest, VehicleModeToStringMapping) {
    EXPECT_EQ(vehicleModeToString(VehicleMode::IDLE),         "IDLE");
    EXPECT_EQ(vehicleModeToString(VehicleMode::DRIVING),      "DRIVING");
    EXPECT_EQ(vehicleModeToString(VehicleMode::ACCELERATING), "ACCELERATING");
    EXPECT_EQ(vehicleModeToString(VehicleMode::BRAKING),      "BRAKING");
    EXPECT_EQ(vehicleModeToString(VehicleMode::FAULT),        "FAULT");
}

TEST(VehicleModelTest, StateJsonContainsAllFields) {
    VehicleModel model;
    model.updateSpeed(65.0);
    model.updateRPM(2350.0, 30.0);
    std::string json = model.getCurrentState().toJson();
    EXPECT_NE(json.find("speed_kmh"),            std::string::npos);
    EXPECT_NE(json.find("rpm"),                  std::string::npos);
    EXPECT_NE(json.find("engine_temperature_c"), std::string::npos);
    EXPECT_NE(json.find("battery_pct"),          std::string::npos);
    EXPECT_NE(json.find("mode"),                 std::string::npos);
}
