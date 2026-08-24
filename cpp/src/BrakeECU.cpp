#include "BrakeECU.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cmath>

namespace vecu {

BrakeECU::BrakeECU(CANBus& bus, int update_hz)
    : VirtualECU("BRAKE_ECU", 2, "0x20", update_hz, bus)
{}

bool BrakeECU::isBrakeActive() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return brake_active_;
}

double BrakeECU::getBrakePressure() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return brake_pressure_;
}

double BrakeECU::getSpeed() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return speed_kmh_;
}

void BrakeECU::setBrake(bool active) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    brake_active_ = active;
}

void BrakeECU::setSpeed(double speed_kmh) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    speed_kmh_ = std::clamp(speed_kmh, 0.0, 250.0);
}

void BrakeECU::setBrakeFailure(bool active, double pressure_factor) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    brake_failure_ = active;
    pressure_response_factor_ = active ? pressure_factor : 1.0;
    if (active) {
        fault_active_.store(true);
        LOG_WARN("BRAKE_ECU", "Brake failure fault ACTIVATED (pressure factor=" +
                 std::to_string(pressure_factor) + ")");
    } else {
        fault_active_.store(false);
        LOG_INFO("BRAKE_ECU", "Brake failure fault DEACTIVATED");
    }
}

void BrakeECU::update() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const double dt = 1.0 / update_frequency_hz_;

    if (brake_active_) {
        // Brake pressure builds toward max (modified by failure factor)
        double target = pressure_max_ * pressure_response_factor_;
        double rise = pressure_rise_rate_ * pressure_response_factor_;
        brake_pressure_ = std::min(brake_pressure_ + rise * dt, target);

        // Speed decreases proportional to effective pressure
        double decel = deceleration_rate_ * (brake_pressure_ / pressure_max_) * dt;
        speed_kmh_ = std::max(0.0, speed_kmh_ - decel * 3.6); // rough conversion
    } else {
        // Pressure bleeds off when brake released
        brake_pressure_ = std::max(0.0, brake_pressure_ - pressure_fall_rate_ * dt);
    }
}

void BrakeECU::generateAndPublish() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 0x201 BRAKE_STATUS
    {
        std::array<uint8_t, 8> data{};
        data[0] = brake_active_ ? 1 : 0;
        data[1] = brake_failure_ ? 0x01 : 0x00;
        CANFrame frame(0x201, 8, data, name_, "BRAKE_STATUS");
        publish(frame);
    }

    // 0x202 BRAKE_PRESSURE (0.1 bar units)
    {
        std::array<uint8_t, 8> data{};
        uint16_t p = static_cast<uint16_t>(brake_pressure_ * 10.0);
        CANFrame::encodeUInt16(data, 0, p);
        CANFrame frame(0x202, 8, data, name_, "BRAKE_PRESSURE");
        publish(frame);
    }

    // 0x203 VEHICLE_SPEED (0.1 km/h units)
    {
        std::array<uint8_t, 8> data{};
        uint16_t s = static_cast<uint16_t>(speed_kmh_ * 10.0);
        CANFrame::encodeUInt16(data, 0, s);
        CANFrame frame(0x203, 8, data, name_, "VEHICLE_SPEED");
        publish(frame);
    }
}

} // namespace vecu
