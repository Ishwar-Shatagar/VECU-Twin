#pragma once
#include "VirtualECU.hpp"

namespace vecu {

/**
 * @brief Virtual Battery ECU.
 *
 * Simulates a high-voltage EV-style battery pack:
 *  - State of charge (% remaining).
 *  - Pack voltage (V).
 *  - Pack temperature (°C).
 *  - Charging state (true = currently charging).
 *
 * Publishes CAN messages:
 *  - 0x301  BATTERY_STATUS   (%, charging flag, fault)
 *  - 0x302  BATTERY_TEMP
 *  - 0x303  BATTERY_VOLTAGE
 *
 * Gradual realistic changes:
 *  - SOC drains slowly during driving.
 *  - Voltage tracks SOC with a realistic lookup.
 *  - Temperature rises slowly under load, drops when idle.
 *
 * Fault modes:
 *  - Battery degradation: accelerated drain rate.
 */
class BatteryECU : public VirtualECU {
public:
    BatteryECU(CANBus& bus, int update_hz = 5);
    ~BatteryECU() override = default;

    // --- State accessors ---
    double getPercentage()   const;
    double getVoltage()      const;
    double getTemperature()  const;
    bool   isCharging()      const;

    // --- Controls ---
    void setCharging(bool charging);
    void setDriveLoad(double load_pct);         ///< affects drain rate
    void setDegradationFault(bool active, double drain_multiplier = 20.0);

protected:
    void update() override;
    void generateAndPublish() override;

private:
    double soc_{84.0};              ///< State of Charge %
    double voltage_{400.0};         ///< Pack voltage V
    double temperature_{25.0};      ///< °C
    bool   charging_{false};
    double drive_load_{0.0};        ///< 0–100

    // Fault
    bool   degradation_fault_{false};
    double drain_multiplier_{1.0};

    // Config
    double voltage_nominal_{400.0};
    double voltage_min_{280.0};
    double voltage_max_{420.0};
    double drain_rate_pct_per_tick_{0.0025};   ///< per update at 5 Hz
    double charge_rate_pct_per_tick_{0.0083};
    double temp_rise_rate_{0.05};
    double temp_cool_rate_{0.02};

    /// Map SOC → voltage (simplified linear approximation)
    double socToVoltage(double soc) const;
};

} // namespace vecu
