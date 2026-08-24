#pragma once
#include "VirtualECU.hpp"
#include <atomic>

namespace vecu {

/**
 * @brief Virtual Brake ECU.
 *
 * Simulates:
 *  - Brake state: active / inactive.
 *  - Brake pressure: builds when brake applied, drains when released.
 *  - Vehicle speed: decreases under braking, can be updated by Engine/scenario.
 *
 * Publishes CAN messages:
 *  - 0x201  BRAKE_STATUS
 *  - 0x202  BRAKE_PRESSURE
 *  - 0x203  VEHICLE_SPEED
 *
 * Fault modes:
 *  - Brake failure: brake_active=true but pressure remains near zero.
 *  - Delayed messages: controlled via base class setMessageDelay().
 */
class BrakeECU : public VirtualECU {
public:
    BrakeECU(CANBus& bus, int update_hz = 20);
    ~BrakeECU() override = default;

    // --- State accessors ---
    bool   isBrakeActive()   const;
    double getBrakePressure() const;
    double getSpeed()         const;

    // --- Controls ---
    void setBrake(bool active);
    void setSpeed(double speed_kmh);
    void setBrakeFailure(bool active, double pressure_factor = 0.05);

protected:
    void update() override;
    void generateAndPublish() override;

private:
    bool   brake_active_{false};
    double brake_pressure_{0.0};   ///< bar
    double speed_kmh_{0.0};

    // Fault
    bool   brake_failure_{false};
    double pressure_response_factor_{1.0};

    // Config
    double pressure_max_{150.0};
    double pressure_rise_rate_{15.0};   ///< bar per update
    double pressure_fall_rate_{10.0};
    double deceleration_rate_{15.0};    ///< km/h per second per bar (normalized)
};

} // namespace vecu
