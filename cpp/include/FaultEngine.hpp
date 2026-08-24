#pragma once
#include "EngineECU.hpp"
#include "BrakeECU.hpp"
#include "BatteryECU.hpp"
#include "SteeringECU.hpp"
#include "VehicleModel.hpp"
#include <string>
#include <functional>
#include <memory>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <thread>
#include <chrono>

namespace vecu {

/**
 * @brief Named scenario identifiers
 */
enum class Scenario {
    NORMAL_DRIVE,
    ACCELERATION,
    BRAKING,
    ENGINE_OVERHEAT,
    BRAKE_FAILURE,
    BATTERY_FAULT,
    COMMUNICATION_LOSS,
    SENSOR_STUCK,
    MIXED_FAULT,
    NONE
};

std::string scenarioToString(Scenario s);
Scenario scenarioFromString(const std::string& s);

/**
 * @brief Fault injection and scenario orchestration engine.
 *
 * The FaultEngine controls:
 *  - Named fault injection (overheat, RPM fault, brake failure, etc.)
 *  - Scenario playback (sequences of state changes over time)
 *  - Clean start/stop of each fault/scenario with logging
 *
 * Every fault is:
 *  - Identified by name.
 *  - Startable and stoppable independently.
 *  - Logged on activation and deactivation.
 *  - Detectable by the FaultDetector (separate class).
 *
 * The FaultEngine does NOT detect faults — it only injects them.
 * Detection is the responsibility of FaultDetector.
 */
class FaultEngine {
public:
    FaultEngine(EngineECU&   engine,
                BrakeECU&    brake,
                BatteryECU&  battery,
                SteeringECU& steering,
                VehicleModel& model);

    ~FaultEngine();

    // --- Scenario control ---
    void startScenario(Scenario s);
    void stopScenario();
    Scenario getActiveScenario() const { return active_scenario_.load(); }
    std::string getActiveScenarioString() const { return scenarioToString(active_scenario_.load()); }

    // --- Individual fault control ---
    void startFault(const std::string& fault_id);
    void stopFault(const std::string& fault_id);
    bool isFaultActive(const std::string& fault_id) const;

    std::vector<std::string> getActiveFaults() const;

private:
    // Scenario implementations
    void applyNormalDrive();
    void applyAcceleration();
    void applyBraking();
    void applyEngineOverheat();
    void applyBrakeFailure();
    void applyBatteryFault();
    void applyCommunicationLoss();
    void applySensorStuck();
    void applyMixedFault();
    void stopAllFaults();

    // Scenario thread (runs scenario over time)
    void scenarioLoop();

    EngineECU&    engine_;
    BrakeECU&     brake_;
    BatteryECU&   battery_;
    SteeringECU&  steering_;
    VehicleModel& model_;

    std::atomic<Scenario> active_scenario_{Scenario::NONE};

    mutable std::mutex faults_mutex_;
    std::unordered_map<std::string, bool> active_faults_;

    std::thread scenario_thread_;
    std::atomic<bool> scenario_running_{false};
};

} // namespace vecu
