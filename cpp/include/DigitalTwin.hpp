#pragma once
#include "VehicleModel.hpp"
#include "CANFrame.hpp"
#include <deque>
#include <mutex>
#include <string>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace vecu {

/// Digital Twin health status
enum class TwinHealth { HEALTHY, WARNING, FAULT };
std::string twinHealthToString(TwinHealth h);

/**
 * @brief Digital Twin of the simulated vehicle.
 *
 * The Digital Twin is NOT a simple copy of the VehicleModel.
 * It represents a SYNCHRONIZED REPRESENTATION built from received
 * ECU messages — exactly as a real Digital Twin is built from
 * telemetry data in a production system.
 *
 * Distinction:
 *  - VehicleModel: the ground-truth simulator (internal state machine).
 *  - DigitalTwin:  the representation built by processing CAN messages,
 *                  validating them, detecting anomalies, and tracking sync.
 *
 * In production: the Digital Twin would receive data from real ECUs
 * over a real CAN bus. Here, it receives the same software CAN frames
 * and builds its state independently from the simulator.
 *
 * Key responsibilities:
 *  1. Receive CAN frames and decode them into state values.
 *  2. Validate values against expected ranges.
 *  3. Maintain current & historical twin state.
 *  4. Track ECU synchronization status (last seen, timeout).
 *  5. Expose health: HEALTHY / WARNING / FAULT.
 *  6. Detect mismatches between expected and observed values.
 *  7. Emit events for significant state changes.
 */
class DigitalTwin {
public:
    DigitalTwin();

    /// Process an incoming CAN frame and update twin state
    void processCANFrame(const CANFrame& frame);

    // --- State queries ---
    VehicleState getCurrentState() const;
    TwinHealth   getHealth() const;
    std::string  getHealthString() const { return twinHealthToString(getHealth()); }

    struct SyncStatus {
        double  last_update_s;
        double  update_age_ms;
        bool    synchronized;
        std::unordered_map<std::string, double> ecu_last_seen;
        std::unordered_map<std::string, bool>   ecu_synced;
    };

    SyncStatus getSyncStatus() const;

    /// Historical twin states (most recent first)
    std::deque<VehicleState> getHistory(int max = 100) const;

    /// Set ECU timeout threshold (default 2000 ms)
    void setECUTimeout(int timeout_ms) { ecu_timeout_ms_ = timeout_ms; }

    /// Count of validation errors since start
    uint64_t getValidationErrors() const { return validation_errors_.load(); }

    std::string toStatusJson() const;

private:
    // CAN frame decoders per message name
    void decodeEngineRPM(const CANFrame& f);
    void decodeEngineTemp(const CANFrame& f);
    void decodeEngineStatus(const CANFrame& f);
    void decodeBrakeStatus(const CANFrame& f);
    void decodeBrakePressure(const CANFrame& f);
    void decodeVehicleSpeed(const CANFrame& f);
    void decodeBatteryStatus(const CANFrame& f);
    void decodeBatteryTemp(const CANFrame& f);
    void decodeBatteryVoltage(const CANFrame& f);
    void decodeSteeringAngle(const CANFrame& f);
    void decodeSteeringStatus(const CANFrame& f);

    bool validateRange(double value, double min, double max, const std::string& field);

    mutable std::mutex state_mutex_;
    VehicleState       state_;
    std::deque<VehicleState> history_;

    // ECU sync tracking
    mutable std::mutex sync_mutex_;
    std::unordered_map<std::string, double> ecu_last_seen_;  ///< ECU name → timestamp

    int ecu_timeout_ms_{2000};

    std::atomic<uint64_t> validation_errors_{0};
    std::atomic<uint64_t> frames_processed_{0};

    static constexpr int HISTORY_MAX = 500;
};

} // namespace vecu
