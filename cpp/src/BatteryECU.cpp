#include "BatteryECU.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cmath>

namespace vecu {

BatteryECU::BatteryECU(CANBus& bus, int update_hz)
    : VirtualECU("BATTERY_ECU", 3, "0x30", update_hz, bus)
{}

double BatteryECU::getPercentage()  const {
    std::lock_guard<std::mutex> lock(state_mutex_); return soc_;
}
double BatteryECU::getVoltage()     const {
    std::lock_guard<std::mutex> lock(state_mutex_); return voltage_;
}
double BatteryECU::getTemperature() const {
    std::lock_guard<std::mutex> lock(state_mutex_); return temperature_;
}
bool BatteryECU::isCharging()       const {
    std::lock_guard<std::mutex> lock(state_mutex_); return charging_;
}

void BatteryECU::setCharging(bool charging) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    charging_ = charging;
}

void BatteryECU::setDriveLoad(double load_pct) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    drive_load_ = std::clamp(load_pct, 0.0, 100.0);
}

void BatteryECU::setDegradationFault(bool active, double drain_multiplier) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    degradation_fault_ = active;
    drain_multiplier_ = active ? drain_multiplier : 1.0;
    if (active) {
        fault_active_.store(true);
        LOG_WARN("BATTERY_ECU", "Degradation fault ACTIVATED (drain x" +
                 std::to_string(drain_multiplier) + ")");
    } else {
        fault_active_.store(false);
        LOG_INFO("BATTERY_ECU", "Degradation fault DEACTIVATED");
    }
}

double BatteryECU::socToVoltage(double soc) const {
    // Simplified linear map: 0% → 280V, 100% → 420V
    return voltage_min_ + (soc / 100.0) * (voltage_max_ - voltage_min_);
}

void BatteryECU::update() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (charging_) {
        soc_ = std::min(100.0, soc_ + charge_rate_pct_per_tick_);
        temperature_ = std::max(20.0, temperature_ - temp_cool_rate_);
    } else {
        double effective_drain = drain_rate_pct_per_tick_
            * (1.0 + drive_load_ / 100.0)
            * drain_multiplier_;
        soc_ = std::max(0.0, soc_ - effective_drain);
        temperature_ += temp_rise_rate_ * (drive_load_ / 100.0 + 0.01);
    }

    temperature_ = std::clamp(temperature_, 15.0, 80.0);
    voltage_     = socToVoltage(soc_);
}

void BatteryECU::generateAndPublish() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 0x301 BATTERY_STATUS
    {
        std::array<uint8_t, 8> data{};
        data[0] = static_cast<uint8_t>(std::clamp(soc_, 0.0, 100.0));
        data[1] = charging_ ? 1 : 0;
        data[2] = degradation_fault_ ? 0x01 : 0x00;
        CANFrame frame(0x301, 8, data, name_, "BATTERY_STATUS");
        publish(frame);
    }

    // 0x302 BATTERY_TEMP (0.1°C)
    {
        std::array<uint8_t, 8> data{};
        int16_t t = static_cast<int16_t>(temperature_ * 10.0);
        CANFrame::encodeInt16(data, 0, t);
        CANFrame frame(0x302, 8, data, name_, "BATTERY_TEMP");
        publish(frame);
    }

    // 0x303 BATTERY_VOLTAGE (0.1V)
    {
        std::array<uint8_t, 8> data{};
        uint16_t v = static_cast<uint16_t>(voltage_ * 10.0);
        CANFrame::encodeUInt16(data, 0, v);
        CANFrame frame(0x303, 8, data, name_, "BATTERY_VOLTAGE");
        publish(frame);
    }
}

} // namespace vecu
