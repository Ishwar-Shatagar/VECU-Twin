#include "SteeringECU.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cmath>

namespace vecu {

SteeringECU::SteeringECU(CANBus& bus, int update_hz)
    : VirtualECU("STEERING_ECU", 4, "0x40", update_hz, bus)
{}

double SteeringECU::getAngle() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return current_angle_;
}

SteeringDirection SteeringECU::getDirection() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return direction_;
}

bool SteeringECU::isAtSafeLimit() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return std::abs(current_angle_) > angle_safe_;
}

void SteeringECU::setTargetAngle(double angle_deg) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    target_angle_ = std::clamp(angle_deg, -angle_max_, angle_max_);
}

void SteeringECU::setAngleRate(double rate_deg_per_s) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    angle_rate_ = std::max(0.0, rate_deg_per_s);
}

void SteeringECU::update() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    const double dt = 1.0 / update_frequency_hz_;

    double diff = target_angle_ - current_angle_;
    double step = angle_rate_ * dt;

    if (std::abs(diff) <= step) {
        current_angle_ = target_angle_;
    } else {
        current_angle_ += (diff > 0 ? step : -step);
    }

    current_angle_ = std::clamp(current_angle_, -angle_max_, angle_max_);

    if (current_angle_ > 1.0) direction_ = SteeringDirection::RIGHT;
    else if (current_angle_ < -1.0) direction_ = SteeringDirection::LEFT;
    else direction_ = SteeringDirection::CENTER;

    if (std::abs(current_angle_) > angle_safe_) {
        fault_active_.store(true);
        LOG_WARN("STEERING_ECU", "Angle exceeds safe limit: " + std::to_string(current_angle_) + "°");
    } else {
        fault_active_.store(false);
    }
}

void SteeringECU::generateAndPublish() {
    std::lock_guard<std::mutex> lock(state_mutex_);

    // 0x401 STEERING_ANGLE (0.1° units, signed)
    {
        std::array<uint8_t, 8> data{};
        int16_t a = static_cast<int16_t>(current_angle_ * 10.0);
        CANFrame::encodeInt16(data, 0, a);
        CANFrame frame(0x401, 8, data, name_, "STEERING_ANGLE");
        publish(frame);
    }

    // 0x402 STEERING_STATUS
    {
        std::array<uint8_t, 8> data{};
        data[0] = static_cast<uint8_t>(static_cast<int>(direction_) + 1); // 0=LEFT, 1=CENTER, 2=RIGHT
        data[1] = (std::abs(current_angle_) > angle_safe_) ? 0x01 : 0x00;
        CANFrame frame(0x402, 8, data, name_, "STEERING_STATUS");
        publish(frame);
    }
}

} // namespace vecu
