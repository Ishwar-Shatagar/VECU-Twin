#include "EngineECU.hpp"
#include "Logger.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>

namespace vecu {

std::string engineStateToString(EngineState state) {
    switch (state) {
        case EngineState::OFF:       return "OFF";
        case EngineState::CRANKING:  return "CRANKING";
        case EngineState::IDLE:      return "IDLE";
        case EngineState::RUNNING:   return "RUNNING";
        case EngineState::HIGH_LOAD: return "HIGH_LOAD";
        case EngineState::FAULT:     return "FAULT";
    }
    return "UNKNOWN";
}

EngineECU::EngineECU(CANBus& bus, int update_hz)
    : VirtualECU("ENGINE_ECU", 1, "0x10", update_hz, bus)
{
    rpm_         = rpm_idle_;
    temperature_ = 20.0;
    load_        = 5.0;
    engine_state_ = EngineState::IDLE;
}

double EngineECU::getRPM() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return rpm_;
}

double EngineECU::getTemperature() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return temperature_;
}

double EngineECU::getLoad() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return load_;
}

EngineState EngineECU::getEngineState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return engine_state_;
}

void EngineECU::setTargetLoad(double load_pct) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    target_load_ = std::clamp(load_pct, 0.0, 100.0);
}

void EngineECU::setOverheatFault(bool active, double heat_rate_mult) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    overheat_fault_ = active;
    heat_rate_mult_ = active ? heat_rate_mult : 1.0;
    if (active) {
        fault_active_.store(true);
        LOG_WARN("ENGINE_ECU", "Overheat fault ACTIVATED (rate x" + std::to_string(heat_rate_mult) + ")");
    } else {
        fault_active_.store(false);
        LOG_INFO("ENGINE_ECU", "Overheat fault DEACTIVATED");
    }
}

void EngineECU::setRPMFault(bool active, double target_rpm) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    rpm_fault_ = active;
    rpm_fault_target_ = target_rpm;
    if (active) {
        fault_active_.store(true);
        LOG_WARN("ENGINE_ECU", "RPM fault ACTIVATED (target=" + std::to_string(target_rpm) + ")");
    } else {
        fault_active_.store(false);
        LOG_INFO("ENGINE_ECU", "RPM fault DEACTIVATED");
    }
}

void EngineECU::setStuckValue(bool active) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    stuck_value_ = active;
    if (active) {
        stuck_rpm_ = rpm_;
        fault_active_.store(true);
        LOG_WARN("ENGINE_ECU", "Stuck-value fault ACTIVATED (RPM frozen at " + std::to_string(stuck_rpm_) + ")");
    } else {
        fault_active_.store(false);
        LOG_INFO("ENGINE_ECU", "Stuck-value fault DEACTIVATED");
    }
}

void EngineECU::receiveMessage(const CANFrame& frame) {
    // Engine ECU can receive commands from Gateway or other ECUs
    // For this simulation, we don't process incoming messages
    (void)frame;
}

/**
 * @brief Advance engine simulation state by one tick.
 *
 * Realistic relationships:
 *  - Target RPM = rpm_idle + (target_load / 100) * (rpm_max - rpm_idle)
 *  - Actual RPM moves toward target at rpm_ramp_rate_ per tick
 *  - Temperature rises faster at higher load, slower at low load
 *  - Temperature cools toward ambient when load is low
 */
void EngineECU::update() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    const double dt = 1.0 / update_frequency_hz_;

    if (stuck_value_) {
        rpm_ = stuck_rpm_;
        // Temperature still changes (it's a different sensor from RPM)
        // but pretend the sensor is also stuck — keep temperature too
        return;
    }

    // --- RPM simulation ---
    double target_rpm = rpm_idle_;
    if (rpm_fault_) {
        target_rpm = rpm_fault_target_;
    } else {
        target_rpm = rpm_idle_ + (target_load_ / 100.0) * (rpm_max_ - rpm_idle_);
    }

    // Ramp RPM toward target
    double rpm_diff = target_rpm - rpm_;
    double ramp = std::min(std::abs(rpm_diff), rpm_ramp_rate_ * dt * 10.0);
    rpm_ += (rpm_diff > 0 ? ramp : -ramp);
    rpm_ = std::clamp(rpm_, 0.0, rpm_max_);

    // --- Load from RPM ---
    load_ = ((rpm_ - rpm_idle_) / (rpm_max_ - rpm_idle_)) * 100.0;
    load_ = std::clamp(load_, 0.0, 100.0);

    // --- Temperature simulation ---
    double effective_heat = heat_rate_ * (load_ / 100.0 + 0.1);
    if (overheat_fault_) {
        effective_heat *= heat_rate_mult_;
    }

    double normal_max_temp = 90.0;
    if (temperature_ < normal_max_temp || overheat_fault_) {
        temperature_ += effective_heat * dt;
    } else {
        // Cool toward normal operating temperature
        double cool = cool_rate_ * dt;
        temperature_ = std::max(temperature_ - cool, normal_max_temp);
    }
    temperature_ = std::clamp(temperature_, 20.0, 200.0);

    // --- Engine state ---
    if (temperature_ >= temp_critical_) {
        engine_state_ = EngineState::FAULT;
    } else if (load_ > 70.0) {
        engine_state_ = EngineState::HIGH_LOAD;
    } else if (load_ > 10.0) {
        engine_state_ = EngineState::RUNNING;
    } else {
        engine_state_ = EngineState::IDLE;
    }
}

void EngineECU::generateAndPublish() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // --- 0x101 ENGINE_RPM ---
    {
        std::array<uint8_t, 8> data{};
        uint16_t rpm_enc = static_cast<uint16_t>(std::clamp(rpm_, 0.0, 65535.0));
        CANFrame::encodeUInt16(data, 0, rpm_enc);
        data[2] = static_cast<uint8_t>(std::clamp(load_, 0.0, 100.0));
        data[3] = static_cast<uint8_t>(engine_state_);
        CANFrame frame(0x101, 8, data, name_, "ENGINE_RPM");
        publish(frame);
    }

    // --- 0x102 ENGINE_TEMP (every other tick at 10 Hz = ~5 Hz) ---
    {
        std::array<uint8_t, 8> data{};
        // Encode temperature as 0.1°C units (int16)
        int16_t temp_enc = static_cast<int16_t>(temperature_ * 10.0);
        CANFrame::encodeInt16(data, 0, temp_enc);
        CANFrame frame(0x102, 8, data, name_, "ENGINE_TEMP");
        publish(frame);
    }

    // --- 0x103 ENGINE_STATUS ---
    {
        std::array<uint8_t, 8> data{};
        data[0] = static_cast<uint8_t>(engine_state_);
        data[1] = overheat_fault_ ? 0x01 : 0x00;
        data[1] |= rpm_fault_ ? 0x02 : 0x00;
        data[1] |= stuck_value_ ? 0x04 : 0x00;
        CANFrame frame(0x103, 8, data, name_, "ENGINE_STATUS");
        publish(frame);
    }
}

} // namespace vecu
