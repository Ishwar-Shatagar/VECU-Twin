#pragma once
#include <string>
#include <mutex>
#include <atomic>
#include <deque>
#include <chrono>

namespace vecu {

/// Operational mode of the simulated vehicle
enum class VehicleMode {
    IDLE,
    DRIVING,
    ACCELERATING,
    BRAKING,
    FAULT
};

std::string vehicleModeToString(VehicleMode mode);
VehicleMode vehicleModeFromString(const std::string& s);

/**
 * @brief Aggregated vehicle state.
 *
 * Holds the current simulated values of all vehicle subsystems.
 * Updated by the VehicleModel when ECU data is parsed from the CAN bus.
 */
struct VehicleState {
    double speed_kmh{0.0};
    double rpm{800.0};
    double engine_temperature_c{20.0};
    double engine_load_pct{0.0};
    bool   brake_active{false};
    double brake_pressure_bar{0.0};
    double battery_pct{84.0};
    double battery_voltage_v{400.0};
    double battery_temperature_c{25.0};
    bool   battery_charging{false};
    double steering_angle_deg{0.0};
    VehicleMode mode{VehicleMode::IDLE};
    double timestamp{0.0};

    std::string toJson() const;
};

/**
 * @brief Vehicle state machine and state aggregator.
 *
 * The VehicleModel receives parsed ECU values (speed, RPM, temp, etc.)
 * and maintains the current VehicleState. It also implements the mode
 * finite-state machine:
 *
 *   IDLE → ACCELERATING → DRIVING → BRAKING → DRIVING
 *                                            → IDLE (full stop)
 *   Any state → FAULT (if critical fault detected)
 *
 * Mode transitions are based on observable parameters, not direct commands.
 * This mirrors how real vehicle software infers driving mode from sensor data.
 */
class VehicleModel {
public:
    VehicleModel();

    // --- Update from ECU decoded values ---
    void updateSpeed(double speed_kmh);
    void updateRPM(double rpm, double load_pct);
    void updateEngineTemp(double temp_c);
    void updateBrake(bool active, double pressure_bar);
    void updateBattery(double pct, double voltage, double temp, bool charging);
    void updateSteering(double angle_deg);
    void setFaultMode(bool fault_active);

    // --- State access ---
    VehicleState getCurrentState() const;
    VehicleMode  getMode() const;

    // --- History ---
    std::deque<VehicleState> getHistory(int max_entries = 100) const;

    static constexpr int HISTORY_MAX = 500;

private:
    void updateMode();

    mutable std::mutex mutex_;
    VehicleState       current_;
    std::deque<VehicleState> history_;
    bool fault_mode_{false};
};

} // namespace vecu
