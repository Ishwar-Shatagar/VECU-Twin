#include "FaultEngine.hpp"
#include "Logger.hpp"
#include <chrono>
#include <thread>
#include <algorithm>

namespace vecu {

std::string scenarioToString(Scenario s) {
    switch (s) {
        case Scenario::NORMAL_DRIVE:       return "NORMAL_DRIVE";
        case Scenario::ACCELERATION:       return "ACCELERATION";
        case Scenario::BRAKING:            return "BRAKING";
        case Scenario::ENGINE_OVERHEAT:    return "ENGINE_OVERHEAT";
        case Scenario::BRAKE_FAILURE:      return "BRAKE_FAILURE";
        case Scenario::BATTERY_FAULT:      return "BATTERY_FAULT";
        case Scenario::COMMUNICATION_LOSS: return "COMMUNICATION_LOSS";
        case Scenario::SENSOR_STUCK:       return "SENSOR_STUCK";
        case Scenario::MIXED_FAULT:        return "MIXED_FAULT";
        case Scenario::NONE:               return "NONE";
    }
    return "NONE";
}

Scenario scenarioFromString(const std::string& s) {
    if (s == "NORMAL_DRIVE")       return Scenario::NORMAL_DRIVE;
    if (s == "ACCELERATION")       return Scenario::ACCELERATION;
    if (s == "BRAKING")            return Scenario::BRAKING;
    if (s == "ENGINE_OVERHEAT")    return Scenario::ENGINE_OVERHEAT;
    if (s == "BRAKE_FAILURE")      return Scenario::BRAKE_FAILURE;
    if (s == "BATTERY_FAULT")      return Scenario::BATTERY_FAULT;
    if (s == "COMMUNICATION_LOSS") return Scenario::COMMUNICATION_LOSS;
    if (s == "SENSOR_STUCK")       return Scenario::SENSOR_STUCK;
    if (s == "MIXED_FAULT")        return Scenario::MIXED_FAULT;
    return Scenario::NONE;
}

FaultEngine::FaultEngine(EngineECU&   engine,
                          BrakeECU&    brake,
                          BatteryECU&  battery,
                          SteeringECU& steering,
                          VehicleModel& model)
    : engine_(engine)
    , brake_(brake)
    , battery_(battery)
    , steering_(steering)
    , model_(model)
{}

FaultEngine::~FaultEngine() {
    stopScenario();
}

void FaultEngine::startScenario(Scenario s) {
    stopScenario();
    active_scenario_.store(s);
    LOG_INFO("FaultEngine", "Scenario started: " + scenarioToString(s));

    scenario_running_.store(true);
    scenario_thread_ = std::thread(&FaultEngine::scenarioLoop, this);
}

void FaultEngine::stopScenario() {
    scenario_running_.store(false);
    if (scenario_thread_.joinable()) {
        scenario_thread_.join();
    }
    stopAllFaults();
    active_scenario_.store(Scenario::NONE);
    LOG_INFO("FaultEngine", "Scenario stopped");
}

void FaultEngine::stopAllFaults() {
    engine_.setOverheatFault(false);
    engine_.setRPMFault(false);
    engine_.setStuckValue(false);
    engine_.setSilenced(false);
    engine_.setMessageDelay(0);
    brake_.setBrakeFailure(false);
    brake_.setSilenced(false);
    brake_.setMessageDelay(0);
    battery_.setDegradationFault(false);
    model_.setFaultMode(false);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_.clear();
}

void FaultEngine::scenarioLoop() {
    while (scenario_running_.load()) {
        Scenario current = active_scenario_.load();
        switch (current) {
            case Scenario::NORMAL_DRIVE:       applyNormalDrive();       break;
            case Scenario::ACCELERATION:       applyAcceleration();      break;
            case Scenario::BRAKING:            applyBraking();           break;
            case Scenario::ENGINE_OVERHEAT:    applyEngineOverheat();    break;
            case Scenario::BRAKE_FAILURE:      applyBrakeFailure();      break;
            case Scenario::BATTERY_FAULT:      applyBatteryFault();      break;
            case Scenario::COMMUNICATION_LOSS: applyCommunicationLoss(); break;
            case Scenario::SENSOR_STUCK:       applySensorStuck();       break;
            case Scenario::MIXED_FAULT:        applyMixedFault();        break;
            case Scenario::NONE:               break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void FaultEngine::applyNormalDrive() {
    // Stable cruise at moderate speed and load
    engine_.setTargetLoad(30.0);
    brake_.setBrake(false);
    brake_.setSpeed(65.0);
    battery_.setDriveLoad(30.0);
    steering_.setTargetAngle(0.0);
}

void FaultEngine::applyAcceleration() {
    // Gradual load increase — speed rises via brake ECU
    static double load = 30.0;
    load = std::min(load + 0.5, 80.0);
    engine_.setTargetLoad(load);
    brake_.setBrake(false);
    double speed = 30.0 + (load - 30.0) * 2.0;
    brake_.setSpeed(std::min(speed, 150.0));
    battery_.setDriveLoad(load);
}

void FaultEngine::applyBraking() {
    engine_.setTargetLoad(10.0);
    brake_.setBrake(true);
    battery_.setDriveLoad(10.0);
}

void FaultEngine::applyEngineOverheat() {
    engine_.setTargetLoad(70.0);
    engine_.setOverheatFault(true, 5.0);
    model_.setFaultMode(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["ENGINE_OVERHEAT"] = true;
}

void FaultEngine::applyBrakeFailure() {
    brake_.setBrake(true);
    brake_.setBrakeFailure(true, 0.05);
    model_.setFaultMode(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["BRAKE_FAILURE"] = true;
}

void FaultEngine::applyBatteryFault() {
    battery_.setDegradationFault(true, 20.0);
    model_.setFaultMode(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["BATTERY_DEGRADATION"] = true;
}

void FaultEngine::applyCommunicationLoss() {
    engine_.setSilenced(true);
    model_.setFaultMode(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["COMMUNICATION_LOSS"] = true;
}

void FaultEngine::applySensorStuck() {
    engine_.setStuckValue(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["SENSOR_STUCK"] = true;
}

void FaultEngine::applyMixedFault() {
    // Two faults simultaneously: engine overheat + battery degradation
    engine_.setOverheatFault(true, 4.0);
    battery_.setDegradationFault(true, 15.0);
    model_.setFaultMode(true);

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_["ENGINE_OVERHEAT"] = true;
    active_faults_["BATTERY_DEGRADATION"] = true;
}

void FaultEngine::startFault(const std::string& fault_id) {
    LOG_INFO("FaultEngine", "Starting individual fault: " + fault_id);
    if (fault_id == "ENGINE_OVERHEAT")    applyEngineOverheat();
    else if (fault_id == "ENGINE_RPM_FAULT") {
        engine_.setRPMFault(true, 7500.0);
        std::lock_guard<std::mutex> lock(faults_mutex_);
        active_faults_["ENGINE_RPM_FAULT"] = true;
    }
    else if (fault_id == "BRAKE_FAILURE")     applyBrakeFailure();
    else if (fault_id == "BATTERY_DEGRADATION") applyBatteryFault();
    else if (fault_id == "SENSOR_STUCK")      applySensorStuck();
    else if (fault_id == "COMMUNICATION_LOSS") applyCommunicationLoss();
    else if (fault_id == "ECU_DELAYED") {
        brake_.setMessageDelay(1500);
        std::lock_guard<std::mutex> lock(faults_mutex_);
        active_faults_["ECU_DELAYED"] = true;
    }
    else if (fault_id == "INCONSISTENT_STATE") {
        engine_.setRPMFault(true, 6000.0);
        brake_.setSpeed(5.0);
        std::lock_guard<std::mutex> lock(faults_mutex_);
        active_faults_["INCONSISTENT_STATE"] = true;
    }
}

void FaultEngine::stopFault(const std::string& fault_id) {
    LOG_INFO("FaultEngine", "Stopping individual fault: " + fault_id);
    if (fault_id == "ENGINE_OVERHEAT")     engine_.setOverheatFault(false);
    else if (fault_id == "ENGINE_RPM_FAULT") engine_.setRPMFault(false);
    else if (fault_id == "BRAKE_FAILURE")   brake_.setBrakeFailure(false);
    else if (fault_id == "BATTERY_DEGRADATION") battery_.setDegradationFault(false);
    else if (fault_id == "SENSOR_STUCK")   engine_.setStuckValue(false);
    else if (fault_id == "COMMUNICATION_LOSS") engine_.setSilenced(false);
    else if (fault_id == "ECU_DELAYED")    brake_.setMessageDelay(0);
    else if (fault_id == "INCONSISTENT_STATE") {
        engine_.setRPMFault(false);
    }

    std::lock_guard<std::mutex> lock(faults_mutex_);
    active_faults_.erase(fault_id);

    // Clear fault mode if no active faults remain
    if (active_faults_.empty()) {
        model_.setFaultMode(false);
    }
}

bool FaultEngine::isFaultActive(const std::string& fault_id) const {
    std::lock_guard<std::mutex> lock(faults_mutex_);
    auto it = active_faults_.find(fault_id);
    return it != active_faults_.end() && it->second;
}

std::vector<std::string> FaultEngine::getActiveFaults() const {
    std::lock_guard<std::mutex> lock(faults_mutex_);
    std::vector<std::string> result;
    for (const auto& [id, active] : active_faults_) {
        if (active) result.push_back(id);
    }
    return result;
}

} // namespace vecu
