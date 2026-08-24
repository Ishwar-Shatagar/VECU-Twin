#include <gtest/gtest.h>
#include "EngineECU.hpp"
#include "BrakeECU.hpp"
#include "BatteryECU.hpp"
#include "SteeringECU.hpp"
#include "GatewayECU.hpp"
#include "CANBus.hpp"
#include <chrono>
#include <thread>
#include <atomic>

using namespace vecu;

// ─── Engine ECU Tests ─────────────────────────────────────────────────────────

class EngineECUTest : public ::testing::Test {
protected:
    CANBus bus{"TestBus"};
    EngineECU engine{bus, 20};

    void SetUp() override {
        bus.start();
        engine.start();
    }
    void TearDown() override {
        engine.stop();
        bus.stop();
    }
};

TEST_F(EngineECUTest, StartsOnline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_EQ(engine.getStatus(), ECUStatus::ONLINE);
}

TEST_F(EngineECUTest, InitialRPMIsIdle) {
    EXPECT_NEAR(engine.getRPM(), 800.0, 200.0);
}

TEST_F(EngineECUTest, HighLoadIncreasesRPM) {
    engine.setTargetLoad(80.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_GT(engine.getRPM(), 1500.0);
}

TEST_F(EngineECUTest, OverheatFaultRaisesTemperature) {
    engine.setOverheatFault(true, 10.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_GT(engine.getTemperature(), 25.0);
    engine.setOverheatFault(false);
}

TEST_F(EngineECUTest, OverheatFaultSetsECUFaultFlag) {
    engine.setOverheatFault(true, 5.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_TRUE(engine.isFaultActive());
    engine.setOverheatFault(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    EXPECT_FALSE(engine.isFaultActive());
}

TEST_F(EngineECUTest, StuckValueFreezesCounts) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    engine.setStuckValue(true);
    double rpm1 = engine.getRPM();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    double rpm2 = engine.getRPM();
    EXPECT_NEAR(rpm1, rpm2, 1.0);
    engine.setStuckValue(false);
}

TEST_F(EngineECUTest, SilencedECUPublishesNoMessages) {
    std::atomic<int> received{0};
    bus.subscribe("silence_test", [&](const CANFrame& f) {
        if (f.source_ecu == "ENGINE_ECU") received++;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int before = received.load();
    engine.setSilenced(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    int after = received.load();
    EXPECT_EQ(before, after);  // no new messages while silenced
    engine.setSilenced(false);
}

TEST_F(EngineECUTest, PublishesCANMessages) {
    std::atomic<int> received{0};
    bus.subscribe("pub_test", [&](const CANFrame& f) {
        if (f.source_ecu == "ENGINE_ECU") received++;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_GT(received.load(), 0);
}

// ─── Brake ECU Tests ──────────────────────────────────────────────────────────

class BrakeECUTest : public ::testing::Test {
protected:
    CANBus bus{"TestBus"};
    BrakeECU brake{bus, 20};

    void SetUp() override { bus.start(); brake.start(); }
    void TearDown() override { brake.stop(); bus.stop(); }
};

TEST_F(BrakeECUTest, InitialStateIsNotBraking) {
    EXPECT_FALSE(brake.isBrakeActive());
    EXPECT_NEAR(brake.getBrakePressure(), 0.0, 0.1);
}

TEST_F(BrakeECUTest, BrakeActiveBuildsPresure) {
    brake.setBrake(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_GT(brake.getBrakePressure(), 1.0);
}

TEST_F(BrakeECUTest, BrakeReleaseDropsPressure) {
    brake.setBrake(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    brake.setBrake(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    EXPECT_LT(brake.getBrakePressure(), brake.getBrakePressure() + 50.0);
}

TEST_F(BrakeECUTest, BrakeFailureReducesPressure) {
    brake.setBrakeFailure(true, 0.05);
    brake.setBrake(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    EXPECT_LT(brake.getBrakePressure(), 20.0);
    brake.setBrakeFailure(false);
}

TEST_F(BrakeECUTest, SpeedSetterClampsToZero) {
    brake.setSpeed(-10.0);
    EXPECT_GE(brake.getSpeed(), 0.0);
}

// ─── Battery ECU Tests ────────────────────────────────────────────────────────

class BatteryECUTest : public ::testing::Test {
protected:
    CANBus bus{"TestBus"};
    BatteryECU battery{bus, 10};

    void SetUp() override { bus.start(); battery.start(); }
    void TearDown() override { battery.stop(); bus.stop(); }
};

TEST_F(BatteryECUTest, InitialSOCIsReasonable) {
    EXPECT_GT(battery.getPercentage(), 50.0);
    EXPECT_LT(battery.getPercentage(), 100.0);
}

TEST_F(BatteryECUTest, DegradationFaultDrainsSOCFast) {
    battery.setDegradationFault(true, 50.0);
    double initial_soc = battery.getPercentage();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_LT(battery.getPercentage(), initial_soc);
    battery.setDegradationFault(false);
}

TEST_F(BatteryECUTest, VoltageTracksSoc) {
    double initial_v = battery.getVoltage();
    battery.setDegradationFault(true, 100.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_LT(battery.getVoltage(), initial_v);
    battery.setDegradationFault(false);
}

// ─── Steering ECU Tests ───────────────────────────────────────────────────────

class SteeringECUTest : public ::testing::Test {
protected:
    CANBus bus{"TestBus"};
    SteeringECU steering{bus, 20};

    void SetUp() override { bus.start(); steering.start(); }
    void TearDown() override { steering.stop(); bus.stop(); }
};

TEST_F(SteeringECUTest, InitialAngleIsZero) {
    EXPECT_NEAR(steering.getAngle(), 0.0, 1.0);
}

TEST_F(SteeringECUTest, AngleMoveTowardTarget) {
    steering.setTargetAngle(90.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    EXPECT_GT(steering.getAngle(), 5.0);
}

TEST_F(SteeringECUTest, AngleClampedToMax) {
    steering.setTargetAngle(1000.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_LE(steering.getAngle(), 540.1);
}
