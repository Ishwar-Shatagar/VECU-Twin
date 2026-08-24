#include <gtest/gtest.h>
#include "FaultEngine.hpp"
#include "FaultDetector.hpp"
#include "VehicleModel.hpp"
#include "DigitalTwin.hpp"
#include "CANBus.hpp"
#include <thread>
#include <chrono>

using namespace vecu;

// ─── FaultDetector Tests ──────────────────────────────────────────────────────

class FaultDetectorTest : public ::testing::Test {
protected:
    CANBus      bus{"TestBus"};
    EngineECU   engine{bus, 10};
    BrakeECU    brake{bus, 20};
    BatteryECU  battery{bus, 5};
    SteeringECU steering{bus, 20};
    GatewayECU  gateway{bus, 10, 5000};  // long timeout for tests
    VehicleModel model;
    DigitalTwin  twin;

    std::unique_ptr<FaultDetector> detector;

    void SetUp() override {
        bus.start();
        detector = std::make_unique<FaultDetector>(model, twin, gateway);
        // Use tight thresholds for testing
        detector->setEngineOverheatWarning(80.0);
        detector->setEngineOverheatCritical(100.0);
        detector->setRPMWarning(4000.0);
        detector->setRPMCritical(6000.0);
        detector->setBatteryWarning(30.0);
        detector->setBatteryCritical(15.0);
    }

    void TearDown() override {
        bus.stop();
    }
};

TEST_F(FaultDetectorTest, NoFaultsInNormalState) {
    model.updateRPM(2000.0, 25.0);
    model.updateEngineTemp(75.0);
    model.updateBrake(false, 0.0);
    model.updateBattery(80.0, 400.0, 30.0, false);
    model.updateSteering(10.0);

    auto events = detector->evaluate();
    EXPECT_TRUE(events.empty());
}

TEST_F(FaultDetectorTest, DetectsEngineOverheatWarning) {
    model.updateEngineTemp(85.0);  // above warning threshold (80°C)
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "ENGINE_OVERHEAT" && ev.severity == FaultSeverity::WARNING)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, DetectsEngineOverheatCritical) {
    model.updateEngineTemp(110.0);  // above critical threshold (100°C)
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "ENGINE_OVERHEAT" && ev.severity == FaultSeverity::CRITICAL)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, DetectsBrakeFailure) {
    model.updateBrake(true, 1.0);  // brake active, pressure very low → failure
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "BRAKE_FAILURE") found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, NoBrakeFailureWhenPressureOk) {
    model.updateBrake(true, 50.0);  // healthy pressure
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "BRAKE_FAILURE") found = true;
    }
    EXPECT_FALSE(found);
}

TEST_F(FaultDetectorTest, DetectsBatteryLowWarning) {
    model.updateBattery(25.0, 350.0, 30.0, false);  // below 30%
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "BATTERY_LOW" && ev.severity == FaultSeverity::WARNING)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, DetectsBatteryCritical) {
    model.updateBattery(10.0, 290.0, 30.0, false);  // below 15%
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "BATTERY_LOW" && ev.severity == FaultSeverity::CRITICAL)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, DetectsHighRPMWarning) {
    model.updateRPM(5000.0, 80.0);
    auto events = detector->evaluate();
    bool found = false;
    for (const auto& ev : events) {
        if (ev.fault_type == "ENGINE_RPM_HIGH" && ev.severity == FaultSeverity::WARNING)
            found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(FaultDetectorTest, EdgeTriggerNoDuplicates) {
    model.updateEngineTemp(85.0);
    auto events1 = detector->evaluate();
    auto events2 = detector->evaluate();  // same state → should NOT re-emit
    EXPECT_GT(events1.size(), 0u);
    EXPECT_EQ(events2.size(), 0u);  // edge-triggered, not level-triggered
}

TEST_F(FaultDetectorTest, FaultEventHasRequiredFields) {
    model.updateEngineTemp(85.0);
    auto events = detector->evaluate();
    ASSERT_FALSE(events.empty());
    const auto& ev = events[0];
    EXPECT_FALSE(ev.fault_type.empty());
    EXPECT_FALSE(ev.source_ecu.empty());
    EXPECT_FALSE(ev.description.empty());
    EXPECT_FALSE(ev.recommended_action.empty());
    EXPECT_GT(ev.timestamp, 0.0);
}

TEST_F(FaultDetectorTest, TotalDetectedCountIncreases) {
    uint64_t before = detector->getTotalDetected();
    model.updateEngineTemp(85.0);
    detector->evaluate();
    EXPECT_GT(detector->getTotalDetected(), before);
}

TEST_F(FaultDetectorTest, FaultEventJsonIsWellFormed) {
    model.updateEngineTemp(85.0);
    auto events = detector->evaluate();
    ASSERT_FALSE(events.empty());
    std::string json = events[0].toJson();
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
    EXPECT_NE(json.find("fault_type"),   std::string::npos);
    EXPECT_NE(json.find("severity"),     std::string::npos);
    EXPECT_NE(json.find("timestamp"),    std::string::npos);
}

// ─── FaultEngine Scenario Tests ───────────────────────────────────────────────

class FaultEngineTest : public ::testing::Test {
protected:
    CANBus      bus{"TestBus"};
    EngineECU   engine{bus, 10};
    BrakeECU    brake{bus, 20};
    BatteryECU  battery{bus, 5};
    SteeringECU steering{bus, 20};
    VehicleModel model;
    std::unique_ptr<FaultEngine> fault_engine;

    void SetUp() override {
        bus.start();
        engine.start();
        brake.start();
        battery.start();
        steering.start();
        fault_engine = std::make_unique<FaultEngine>(
            engine, brake, battery, steering, model);
    }

    void TearDown() override {
        fault_engine->stopScenario();
        steering.stop();
        battery.stop();
        brake.stop();
        engine.stop();
        bus.stop();
    }
};

TEST_F(FaultEngineTest, NormalDriveScenarioRuns) {
    EXPECT_NO_THROW(fault_engine->startScenario(Scenario::NORMAL_DRIVE));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_EQ(fault_engine->getActiveScenario(), Scenario::NORMAL_DRIVE);
}

TEST_F(FaultEngineTest, ScenarioStopsCleanly) {
    fault_engine->startScenario(Scenario::NORMAL_DRIVE);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_NO_THROW(fault_engine->stopScenario());
    EXPECT_EQ(fault_engine->getActiveScenario(), Scenario::NONE);
}

TEST_F(FaultEngineTest, EngineOverheatActivatesEngineFault) {
    fault_engine->startScenario(Scenario::ENGINE_OVERHEAT);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_TRUE(engine.isFaultActive());
}

TEST_F(FaultEngineTest, IndividualFaultStartStop) {
    fault_engine->startFault("ENGINE_OVERHEAT");
    EXPECT_TRUE(fault_engine->isFaultActive("ENGINE_OVERHEAT"));
    fault_engine->stopFault("ENGINE_OVERHEAT");
    EXPECT_FALSE(fault_engine->isFaultActive("ENGINE_OVERHEAT"));
}

TEST_F(FaultEngineTest, CommunicationLossSilencesECU) {
    fault_engine->startFault("COMMUNICATION_LOSS");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(engine.isSilenced());
    fault_engine->stopFault("COMMUNICATION_LOSS");
    EXPECT_FALSE(engine.isSilenced());
}

TEST_F(FaultEngineTest, ScenarioToStringMapping) {
    EXPECT_EQ(scenarioToString(Scenario::NORMAL_DRIVE),       "NORMAL_DRIVE");
    EXPECT_EQ(scenarioToString(Scenario::ENGINE_OVERHEAT),    "ENGINE_OVERHEAT");
    EXPECT_EQ(scenarioToString(Scenario::BRAKE_FAILURE),      "BRAKE_FAILURE");
    EXPECT_EQ(scenarioToString(Scenario::COMMUNICATION_LOSS), "COMMUNICATION_LOSS");
    EXPECT_EQ(scenarioToString(Scenario::NONE),               "NONE");
}

TEST_F(FaultEngineTest, ScenarioFromStringRoundTrip) {
    EXPECT_EQ(scenarioFromString("NORMAL_DRIVE"),       Scenario::NORMAL_DRIVE);
    EXPECT_EQ(scenarioFromString("ENGINE_OVERHEAT"),    Scenario::ENGINE_OVERHEAT);
    EXPECT_EQ(scenarioFromString("MIXED_FAULT"),        Scenario::MIXED_FAULT);
    EXPECT_EQ(scenarioFromString("INVALID_NAME"),       Scenario::NONE);
}
