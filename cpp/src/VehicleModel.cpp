#include "VehicleModel.hpp"
#include "CANFrame.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace vecu {

std::string vehicleModeToString(VehicleMode mode) {
    switch (mode) {
        case VehicleMode::IDLE:         return "IDLE";
        case VehicleMode::DRIVING:      return "DRIVING";
        case VehicleMode::ACCELERATING: return "ACCELERATING";
        case VehicleMode::BRAKING:      return "BRAKING";
        case VehicleMode::FAULT:        return "FAULT";
    }
    return "UNKNOWN";
}

VehicleMode vehicleModeFromString(const std::string& s) {
    if (s == "IDLE")         return VehicleMode::IDLE;
    if (s == "DRIVING")      return VehicleMode::DRIVING;
    if (s == "ACCELERATING") return VehicleMode::ACCELERATING;
    if (s == "BRAKING")      return VehicleMode::BRAKING;
    if (s == "FAULT")        return VehicleMode::FAULT;
    return VehicleMode::IDLE;
}

std::string VehicleState::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"speed_kmh\":"              << speed_kmh             << ","
       << "\"rpm\":"                    << rpm                   << ","
       << "\"engine_temperature_c\":"   << engine_temperature_c  << ","
       << "\"engine_load_pct\":"        << engine_load_pct       << ","
       << "\"brake_active\":"           << (brake_active ? "true" : "false") << ","
       << "\"brake_pressure_bar\":"     << brake_pressure_bar    << ","
       << "\"battery_pct\":"            << battery_pct           << ","
       << "\"battery_voltage_v\":"      << battery_voltage_v     << ","
       << "\"battery_temperature_c\":"  << battery_temperature_c << ","
       << "\"battery_charging\":"       << (battery_charging ? "true" : "false") << ","
       << "\"steering_angle_deg\":"     << steering_angle_deg    << ","
       << "\"mode\":\""                 << vehicleModeToString(mode) << "\","
       << "\"timestamp\":"              << std::setprecision(3) << timestamp
       << "}";
    return ss.str();
}

// ─── VehicleModel ────────────────────────────────────────────────────────────

VehicleModel::VehicleModel() {
    current_.timestamp = CANFrame::now();
}

void VehicleModel::updateSpeed(double speed_kmh) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.speed_kmh = std::clamp(speed_kmh, 0.0, 250.0);
    current_.timestamp = CANFrame::now();
    updateMode();
    if (history_.size() >= HISTORY_MAX) history_.pop_front();
    history_.push_back(current_);
}

void VehicleModel::updateRPM(double rpm, double load_pct) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.rpm            = std::clamp(rpm, 0.0, 9000.0);
    current_.engine_load_pct = std::clamp(load_pct, 0.0, 100.0);
    current_.timestamp = CANFrame::now();
    updateMode();
}

void VehicleModel::updateEngineTemp(double temp_c) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.engine_temperature_c = std::clamp(temp_c, -40.0, 200.0);
    current_.timestamp = CANFrame::now();
    updateMode();
}

void VehicleModel::updateBrake(bool active, double pressure_bar) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.brake_active       = active;
    current_.brake_pressure_bar = std::clamp(pressure_bar, 0.0, 200.0);
    current_.timestamp = CANFrame::now();
    updateMode();
}

void VehicleModel::updateBattery(double pct, double voltage, double temp, bool charging) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.battery_pct           = std::clamp(pct, 0.0, 100.0);
    current_.battery_voltage_v     = std::clamp(voltage, 0.0, 500.0);
    current_.battery_temperature_c = std::clamp(temp, -40.0, 100.0);
    current_.battery_charging      = charging;
    current_.timestamp = CANFrame::now();
}

void VehicleModel::updateSteering(double angle_deg) {
    std::lock_guard<std::mutex> lock(mutex_);
    current_.steering_angle_deg = std::clamp(angle_deg, -540.0, 540.0);
    current_.timestamp = CANFrame::now();
}

void VehicleModel::setFaultMode(bool fault_active) {
    std::lock_guard<std::mutex> lock(mutex_);
    fault_mode_ = fault_active;
    if (fault_active) current_.mode = VehicleMode::FAULT;
    else updateMode();
}

/**
 * @brief Infer vehicle mode from observable parameters.
 *
 * Mode FSM transitions:
 *   IDLE        → speed < 1 km/h && !brake
 *   BRAKING     → brake active
 *   ACCELERATING → speed increasing (load > 40%)
 *   DRIVING     → otherwise moving
 *   FAULT       → overrides all (set externally)
 */
void VehicleModel::updateMode() {
    if (fault_mode_) {
        current_.mode = VehicleMode::FAULT;
        return;
    }
    if (current_.brake_active) {
        current_.mode = VehicleMode::BRAKING;
    } else if (current_.speed_kmh < 1.0) {
        current_.mode = VehicleMode::IDLE;
    } else if (current_.engine_load_pct > 40.0) {
        current_.mode = VehicleMode::ACCELERATING;
    } else {
        current_.mode = VehicleMode::DRIVING;
    }
}

VehicleState VehicleModel::getCurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

VehicleMode VehicleModel::getMode() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_.mode;
}

std::deque<VehicleState> VehicleModel::getHistory(int max_entries) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (static_cast<int>(history_.size()) <= max_entries) return history_;
    return std::deque<VehicleState>(
        history_.end() - max_entries, history_.end());
}

} // namespace vecu
