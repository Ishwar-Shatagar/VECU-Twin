#pragma once
#include "VirtualECU.hpp"
#include <atomic>

namespace vecu {

/// Engine operational state
enum class EngineState {
    OFF,
    CRANKING,
    IDLE,
    RUNNING,
    HIGH_LOAD,
    FAULT
};

std::string engineStateToString(EngineState state);

/**
 * @brief Virtual Engine ECU.
 *
 * Simulates:
 *  - RPM: starts at idle (~800), rises during acceleration, governed by load.
 *  - Engine temperature: rises under load, cools when idle.
 *  - Engine load: percentage of maximum power output.
 *  - Engine state: OFF → CRANKING → IDLE → RUNNING → HIGH_LOAD → FAULT.
 *
 * Publishes CAN messages:
 *  - 0x101  ENGINE_RPM
 *  - 0x102  ENGINE_TEMP
 *  - 0x103  ENGINE_STATUS
 *
 * Realistic relationships:
 *  - RPM increases with load.
 *  - Temperature rises faster under high load.
 *  - Temperature gradually decreases when load drops.
 */
class EngineECU : public VirtualECU {
public:
    EngineECU(CANBus& bus, int update_hz = 10);
    ~EngineECU() override = default;

    // --- State accessors (thread-safe) ---
    double      getRPM()          const;
    double      getTemperature()  const;
    double      getLoad()         const;
    EngineState getEngineState()  const;

    // --- Scenario controls (called from FaultEngine / main) ---
    void setTargetLoad(double load_pct);     ///< 0.0–100.0
    void setOverheatFault(bool active, double heat_rate_mult = 5.0);
    void setRPMFault(bool active, double target_rpm = 7500.0);
    void setStuckValue(bool active);
    void receiveMessage(const CANFrame& frame) override;

protected:
    void update() override;
    void generateAndPublish() override;

private:
    // Simulated state
    double      rpm_{800.0};
    double      temperature_{20.0};   ///< °C
    double      load_{5.0};           ///< %
    EngineState engine_state_{EngineState::IDLE};

    // Targets and control
    double target_load_{5.0};

    // Fault flags
    bool   overheat_fault_{false};
    double heat_rate_mult_{1.0};
    bool   rpm_fault_{false};
    double rpm_fault_target_{7500.0};
    bool   stuck_value_{false};
    double stuck_rpm_{0.0};

    // Config (loaded from vehicle.json in main, passed in)
    double rpm_idle_{800.0};
    double rpm_max_{8000.0};
    double temp_normal_max_{100.0};
    double temp_warning_{110.0};
    double temp_critical_{130.0};
    double heat_rate_{0.5};
    double cool_rate_{0.3};
    double rpm_ramp_rate_{100.0};   ///< RPM change per update tick
};

} // namespace vecu
