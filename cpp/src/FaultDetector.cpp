#include "FaultDetector.hpp"
#include "CANFrame.hpp"
#include "Logger.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace vecu {

std::string faultSeverityToString(FaultSeverity s) {
    switch (s) {
        case FaultSeverity::NORMAL:   return "NORMAL";
        case FaultSeverity::WARNING:  return "WARNING";
        case FaultSeverity::CRITICAL: return "CRITICAL";
    }
    return "NORMAL";
}

std::string FaultEvent::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"fault_type\":\"" << fault_type << "\","
       << "\"severity\":\"" << faultSeverityToString(severity) << "\","
       << "\"source_ecu\":\"" << source_ecu << "\","
       << "\"timestamp\":" << std::setprecision(3) << timestamp << ","
       << "\"description\":\"" << description << "\","
       << "\"current_value\":" << std::setprecision(2) << current_value << ","
       << "\"expected_min\":" << expected_min << ","
       << "\"expected_max\":" << expected_max << ","
       << "\"recommended_action\":\"" << recommended_action << "\""
       << "}";
    return ss.str();
}

FaultDetector::FaultDetector(const VehicleModel& model,
                              const DigitalTwin& twin,
                              const GatewayECU& gateway)
    : model_(model)
    , twin_(twin)
    , gateway_(gateway)
{}

FaultEvent FaultDetector::makeEvent(const std::string& type,
                                     FaultSeverity severity,
                                     const std::string& ecu,
                                     const std::string& desc,
                                     double current,
                                     double exp_min,
                                     double exp_max,
                                     const std::string& action) const {
    FaultEvent ev;
    ev.fault_type         = type;
    ev.severity           = severity;
    ev.source_ecu         = ecu;
    ev.timestamp          = CANFrame::now();
    ev.description        = desc;
    ev.current_value      = current;
    ev.expected_min       = exp_min;
    ev.expected_max       = exp_max;
    ev.recommended_action = action;
    return ev;
}

/**
 * @brief Evaluate all detection rules against current vehicle state.
 *
 * Rules are evaluated in priority order. Each rule:
 *  1. Reads current state from VehicleModel / DigitalTwin.
 *  2. Compares against configured thresholds.
 *  3. If condition met AND not already active → emit new FaultEvent.
 *  4. If condition cleared → mark inactive (allows re-detection later).
 *
 * Returns only NEW events detected in this call.
 */
std::vector<FaultEvent> FaultDetector::evaluate() {
    std::vector<FaultEvent> new_events;
    VehicleState state = model_.getCurrentState();
    double now = CANFrame::now();

    auto check = [&](const std::string& key, bool condition,
                     std::function<FaultEvent()> make) {
        std::lock_guard<std::mutex> lock(active_mutex_);
        bool was_active = active_flags_.count(key) && active_flags_.at(key);
        if (condition && !was_active) {
            FaultEvent ev = make();
            active_flags_[key] = true;
            new_events.push_back(ev);
            total_detected_.fetch_add(1);
            LOG_WARN("FaultDetector", "[" + faultSeverityToString(ev.severity) + "] " +
                     ev.fault_type + ": " + ev.description);
        } else if (!condition && was_active) {
            active_flags_[key] = false;
        }
    };

    // ── Rule 1: Engine Temperature ─────────────────────────────────────────
    check("ENGINE_OVERHEAT_CRITICAL",
          state.engine_temperature_c >= engine_temp_critical_,
          [&] { return makeEvent("ENGINE_OVERHEAT", FaultSeverity::CRITICAL,
              "ENGINE_ECU",
              "Engine temperature is critically high — risk of engine damage",
              state.engine_temperature_c, 60.0, engine_temp_critical_,
              "Reduce engine load immediately; check cooling system"); });

    check("ENGINE_OVERHEAT_WARNING",
          state.engine_temperature_c >= engine_temp_warning_ &&
          state.engine_temperature_c < engine_temp_critical_,
          [&] { return makeEvent("ENGINE_OVERHEAT", FaultSeverity::WARNING,
              "ENGINE_ECU",
              "Engine temperature approaching critical threshold",
              state.engine_temperature_c, 60.0, engine_temp_warning_,
              "Monitor temperature; reduce engine load"); });

    // ── Rule 2: Engine RPM ─────────────────────────────────────────────────
    check("ENGINE_RPM_CRITICAL",
          state.rpm >= rpm_critical_,
          [&] { return makeEvent("ENGINE_RPM_HIGH", FaultSeverity::CRITICAL,
              "ENGINE_ECU",
              "Engine RPM critically high — potential runaway condition",
              state.rpm, 800.0, rpm_critical_,
              "Check throttle control; reduce load immediately"); });

    check("ENGINE_RPM_WARNING",
          state.rpm >= rpm_warning_ && state.rpm < rpm_critical_,
          [&] { return makeEvent("ENGINE_RPM_HIGH", FaultSeverity::WARNING,
              "ENGINE_ECU",
              "Engine RPM above normal operating range",
              state.rpm, 800.0, rpm_warning_,
              "Monitor RPM; check for throttle malfunction"); });

    // ── Rule 3: Brake Failure ──────────────────────────────────────────────
    check("BRAKE_FAILURE",
          state.brake_active && state.brake_pressure_bar < brake_pressure_min_,
          [&] { return makeEvent("BRAKE_FAILURE", FaultSeverity::CRITICAL,
              "BRAKE_ECU",
              "Brake commanded but pressure is critically low — possible hydraulic failure",
              state.brake_pressure_bar, brake_pressure_min_, 150.0,
              "Emergency: stop vehicle; inspect brake hydraulic system"); });

    // ── Rule 4: Battery Percentage ─────────────────────────────────────────
    check("BATTERY_CRITICAL",
          state.battery_pct <= battery_critical_,
          [&] { return makeEvent("BATTERY_LOW", FaultSeverity::CRITICAL,
              "BATTERY_ECU",
              "Battery charge critically low",
              state.battery_pct, battery_critical_, 100.0,
              "Charge battery immediately; vehicle may shut down"); });

    check("BATTERY_WARNING",
          state.battery_pct <= battery_warning_ && state.battery_pct > battery_critical_,
          [&] { return makeEvent("BATTERY_LOW", FaultSeverity::WARNING,
              "BATTERY_ECU",
              "Battery charge below recommended level",
              state.battery_pct, battery_warning_, 100.0,
              "Charge battery when possible"); });

    // ── Rule 5: Battery Temperature ────────────────────────────────────────
    check("BATTERY_TEMP_CRITICAL",
          state.battery_temperature_c >= battery_temp_critical_,
          [&] { return makeEvent("BATTERY_OVERHEAT", FaultSeverity::CRITICAL,
              "BATTERY_ECU",
              "Battery pack temperature critically high — risk of thermal event",
              state.battery_temperature_c, 0.0, battery_temp_critical_,
              "Stop charging; allow battery to cool; inspect thermal management"); });

    check("BATTERY_TEMP_WARNING",
          state.battery_temperature_c >= battery_temp_warning_ &&
          state.battery_temperature_c < battery_temp_critical_,
          [&] { return makeEvent("BATTERY_OVERHEAT", FaultSeverity::WARNING,
              "BATTERY_ECU",
              "Battery temperature elevated",
              state.battery_temperature_c, 0.0, battery_temp_warning_,
              "Monitor battery temperature; reduce charge rate if charging"); });

    // ── Rule 6: Steering Angle Limit ───────────────────────────────────────
    check("STEERING_FAULT",
          std::abs(state.steering_angle_deg) > steering_safe_,
          [&] { return makeEvent("STEERING_FAULT", FaultSeverity::WARNING,
              "STEERING_ECU",
              "Steering angle exceeds safe operating limit",
              state.steering_angle_deg, -steering_safe_, steering_safe_,
              "Check steering system; do not exceed mechanical stop"); });

    // ── Rule 7: ECU Communication Timeout ─────────────────────────────────
    auto health = gateway_.getECUHealthMap();
    const std::vector<std::string> monitored = {
        "ENGINE_ECU", "BRAKE_ECU", "BATTERY_ECU", "STEERING_ECU"
    };
    for (const auto& ecu_name : monitored) {
        auto it = health.find(ecu_name);
        bool timed_out = (it != health.end()) && it->second.timed_out;
        check("ECU_TIMEOUT_" + ecu_name,
              timed_out,
              [&] { return makeEvent("ECU_COMMUNICATION_LOSS", FaultSeverity::CRITICAL,
                  ecu_name,
                  ecu_name + " has stopped communicating (timeout)",
                  0.0, 0.0, static_cast<double>(ecu_timeout_ms_),
                  "Check " + ecu_name + " power and CAN connection"); });
    }

    // ── Rule 8: Speed/RPM Mismatch ─────────────────────────────────────────
    if (state.speed_kmh > 5.0) {
        double ratio = state.rpm / std::max(state.speed_kmh, 1.0);
        check("SPEED_RPM_MISMATCH",
              ratio > speed_rpm_ratio_max_,
              [&] { return makeEvent("SPEED_RPM_MISMATCH", FaultSeverity::WARNING,
                  "ENGINE_ECU",
                  "RPM/speed ratio inconsistent — possible sensor fault or gear slip",
                  ratio, 0.0, speed_rpm_ratio_max_,
                  "Verify ECU sensor readings; check transmission"); });
    }

    // Store events
    if (!new_events.empty()) {
        std::lock_guard<std::mutex> lock(events_mutex_);
        for (const auto& ev : new_events) {
            if (events_.size() >= EVENTS_MAX) events_.pop_front();
            events_.push_back(ev);
        }
    }

    return new_events;
}

std::deque<FaultEvent> FaultDetector::getAllEvents(int max) const {
    std::lock_guard<std::mutex> lock(events_mutex_);
    if (static_cast<int>(events_.size()) <= max) return events_;
    return std::deque<FaultEvent>(events_.end() - max, events_.end());
}

std::vector<FaultEvent> FaultDetector::getActiveEvents() const {
    std::lock_guard<std::mutex> lock(active_mutex_);
    std::vector<FaultEvent> active;
    std::lock_guard<std::mutex> ev_lock(events_mutex_);
    for (const auto& [key, is_active] : active_flags_) {
        if (is_active) {
            // Find the most recent event with this fault type
            for (auto it = events_.rbegin(); it != events_.rend(); ++it) {
                if (key.find(it->fault_type) != std::string::npos ||
                    it->fault_type + "_" == key.substr(0, it->fault_type.size() + 1)) {
                    active.push_back(*it);
                    break;
                }
            }
        }
    }
    return active;
}

} // namespace vecu
