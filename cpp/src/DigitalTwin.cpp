#include "DigitalTwin.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace vecu {

std::string twinHealthToString(TwinHealth h) {
    switch (h) {
        case TwinHealth::HEALTHY: return "HEALTHY";
        case TwinHealth::WARNING: return "WARNING";
        case TwinHealth::FAULT:   return "FAULT";
    }
    return "UNKNOWN";
}

DigitalTwin::DigitalTwin() {
    state_.timestamp = CANFrame::now();
}

/**
 * @brief Process a CAN frame and update the twin's internal state.
 *
 * The Digital Twin builds its state solely from received CAN frames.
 * It does NOT directly read from the VehicleModel or ECUs.
 * This mirrors how a real Digital Twin operates: it receives telemetry
 * data (here, CAN frames) and reconstructs the vehicle's state from them.
 *
 * If a CAN frame contains out-of-range values, a validation error is
 * logged and counted, but the value is clamped and accepted to maintain
 * continuity (graceful degradation).
 */
void DigitalTwin::processCANFrame(const CANFrame& frame) {
    frames_processed_.fetch_add(1);

    // Update ECU sync tracking
    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        ecu_last_seen_[frame.source_ecu] = frame.timestamp;
    }

    // Dispatch to decoder by message name
    const auto& name = frame.message_name;
    if      (name == "ENGINE_RPM")      decodeEngineRPM(frame);
    else if (name == "ENGINE_TEMP")     decodeEngineTemp(frame);
    else if (name == "ENGINE_STATUS")   decodeEngineStatus(frame);
    else if (name == "BRAKE_STATUS")    decodeBrakeStatus(frame);
    else if (name == "BRAKE_PRESSURE")  decodeBrakePressure(frame);
    else if (name == "VEHICLE_SPEED")   decodeVehicleSpeed(frame);
    else if (name == "BATTERY_STATUS")  decodeBatteryStatus(frame);
    else if (name == "BATTERY_TEMP")    decodeBatteryTemp(frame);
    else if (name == "BATTERY_VOLTAGE") decodeBatteryVoltage(frame);
    else if (name == "STEERING_ANGLE")  decodeSteeringAngle(frame);
    else if (name == "STEERING_STATUS") decodeSteeringStatus(frame);

    // Record state snapshot into history
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.timestamp = CANFrame::now();
    if (history_.size() >= HISTORY_MAX) history_.pop_front();
    history_.push_back(state_);
}

bool DigitalTwin::validateRange(double value, double min, double max, const std::string& field) {
    if (value < min || value > max) {
        validation_errors_.fetch_add(1);
        LOG_WARN("DigitalTwin", "Validation error: " + field +
                 " = " + std::to_string(value) +
                 " out of range [" + std::to_string(min) + ", " + std::to_string(max) + "]");
        return false;
    }
    return true;
}

// ─── CAN Frame Decoders ──────────────────────────────────────────────────────

void DigitalTwin::decodeEngineRPM(const CANFrame& f) {
    uint16_t rpm_raw = CANFrame::decodeUInt16(f.data, 0);
    double rpm = static_cast<double>(rpm_raw);
    validateRange(rpm, 0, 9000, "rpm");
    double load = static_cast<double>(f.data[2]);
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.rpm            = std::clamp(rpm, 0.0, 9000.0);
    state_.engine_load_pct = std::clamp(load, 0.0, 100.0);
}

void DigitalTwin::decodeEngineTemp(const CANFrame& f) {
    int16_t raw = CANFrame::decodeInt16(f.data, 0);
    double temp = raw / 10.0;
    validateRange(temp, -40, 200, "engine_temperature");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.engine_temperature_c = std::clamp(temp, -40.0, 200.0);
}

void DigitalTwin::decodeEngineStatus(const CANFrame& /*f*/) {
    // Status byte decoded elsewhere; nothing extra to update in state
}

void DigitalTwin::decodeBrakeStatus(const CANFrame& f) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.brake_active = (f.data[0] == 1);
}

void DigitalTwin::decodeBrakePressure(const CANFrame& f) {
    uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
    double pressure = raw / 10.0;
    validateRange(pressure, 0, 200, "brake_pressure");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.brake_pressure_bar = std::clamp(pressure, 0.0, 200.0);
}

void DigitalTwin::decodeVehicleSpeed(const CANFrame& f) {
    uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
    double speed = raw / 10.0;
    validateRange(speed, 0, 300, "speed");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.speed_kmh = std::clamp(speed, 0.0, 300.0);
}

void DigitalTwin::decodeBatteryStatus(const CANFrame& f) {
    double pct = static_cast<double>(f.data[0]);
    bool charging = (f.data[1] == 1);
    validateRange(pct, 0, 100, "battery_pct");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.battery_pct     = std::clamp(pct, 0.0, 100.0);
    state_.battery_charging = charging;
}

void DigitalTwin::decodeBatteryTemp(const CANFrame& f) {
    int16_t raw = CANFrame::decodeInt16(f.data, 0);
    double temp = raw / 10.0;
    validateRange(temp, -40, 100, "battery_temperature");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.battery_temperature_c = std::clamp(temp, -40.0, 100.0);
}

void DigitalTwin::decodeBatteryVoltage(const CANFrame& f) {
    uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
    double voltage = raw / 10.0;
    validateRange(voltage, 0, 500, "battery_voltage");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.battery_voltage_v = std::clamp(voltage, 0.0, 500.0);
}

void DigitalTwin::decodeSteeringAngle(const CANFrame& f) {
    int16_t raw = CANFrame::decodeInt16(f.data, 0);
    double angle = raw / 10.0;
    validateRange(angle, -540, 540, "steering_angle");
    std::lock_guard<std::mutex> lock(state_mutex_);
    state_.steering_angle_deg = std::clamp(angle, -540.0, 540.0);
}

void DigitalTwin::decodeSteeringStatus(const CANFrame& /*f*/) {
    // direction already inferred from angle
}

// ─── Health & Sync ───────────────────────────────────────────────────────────

VehicleState DigitalTwin::getCurrentState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

TwinHealth DigitalTwin::getHealth() const {
    // Health is determined by sync status and validation error rate
    auto sync = getSyncStatus();
    bool any_timed_out = false;
    for (const auto& [ecu, synced] : sync.ecu_synced) {
        if (!synced) { any_timed_out = true; break; }
    }
    if (any_timed_out) return TwinHealth::FAULT;
    if (sync.update_age_ms > 1000.0) return TwinHealth::WARNING;
    return TwinHealth::HEALTHY;
}

DigitalTwin::SyncStatus DigitalTwin::getSyncStatus() const {
    SyncStatus s;
    double now = CANFrame::now();

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        s.last_update_s = state_.timestamp;
        s.update_age_ms = (now - state_.timestamp) * 1000.0;
    }

    {
        std::lock_guard<std::mutex> lock(sync_mutex_);
        s.ecu_last_seen = ecu_last_seen_;
        double timeout_s = ecu_timeout_ms_ / 1000.0;
        for (const auto& [ecu, last_seen] : ecu_last_seen_) {
            s.ecu_synced[ecu] = (now - last_seen) < timeout_s;
        }
    }

    s.synchronized = (s.update_age_ms < 1000.0);
    return s;
}

std::deque<VehicleState> DigitalTwin::getHistory(int max) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (static_cast<int>(history_.size()) <= max) return history_;
    return std::deque<VehicleState>(history_.end() - max, history_.end());
}

std::string DigitalTwin::toStatusJson() const {
    auto sync = getSyncStatus();
    auto health = getHealth();
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);
    ss << "{"
       << "\"health\":\"" << twinHealthToString(health) << "\","
       << "\"synchronized\":" << (sync.synchronized ? "true" : "false") << ","
       << "\"update_age_ms\":" << sync.update_age_ms << ","
       << "\"frames_processed\":" << frames_processed_.load() << ","
       << "\"validation_errors\":" << validation_errors_.load() << ","
       << "\"ecu_sync\":{";
    bool first = true;
    for (const auto& [ecu, synced] : sync.ecu_synced) {
        if (!first) ss << ",";
        ss << "\"" << ecu << "\":" << (synced ? "true" : "false");
        first = false;
    }
    ss << "}}";
    return ss.str();
}

} // namespace vecu
