#pragma once
#include "VirtualECU.hpp"

namespace vecu {

/// Steering direction
enum class SteeringDirection { LEFT = -1, CENTER = 0, RIGHT = 1 };

/**
 * @brief Virtual Steering ECU.
 *
 * Simulates:
 *  - Steering angle: ranges −540° to +540° (full lock to lock).
 *  - Steering direction: LEFT, CENTER, RIGHT.
 *  - Safe angle enforcement: emits WARNING if angle exceeds safe limit.
 *
 * Publishes CAN messages:
 *  - 0x401  STEERING_ANGLE
 *  - 0x402  STEERING_STATUS
 *
 * The steering angle changes smoothly when a target is set,
 * demonstrating realistic position control.
 */
class SteeringECU : public VirtualECU {
public:
    SteeringECU(CANBus& bus, int update_hz = 20);
    ~SteeringECU() override = default;

    // --- State accessors ---
    double            getAngle()     const;
    SteeringDirection getDirection() const;
    bool              isAtSafeLimit() const;

    // --- Controls ---
    void setTargetAngle(double angle_deg);    ///< clamped to ±540°
    void setAngleRate(double rate_deg_per_s); ///< how fast angle changes

protected:
    void update() override;
    void generateAndPublish() override;

private:
    double current_angle_{0.0};
    double target_angle_{0.0};
    double angle_rate_{30.0};     ///< degrees per second
    SteeringDirection direction_{SteeringDirection::CENTER};

    // Config
    double angle_max_{540.0};
    double angle_safe_{480.0};
};

} // namespace vecu
