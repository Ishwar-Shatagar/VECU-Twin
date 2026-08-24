#pragma once
#include "VehicleModel.hpp"
#include "DigitalTwin.hpp"
#include "GatewayECU.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <deque>
#include <functional>
#include <chrono>

namespace vecu {

/// Fault severity levels
enum class FaultSeverity { NORMAL, WARNING, CRITICAL };
std::string faultSeverityToString(FaultSeverity s);

/**
 * @brief A detected fault event.
 */
struct FaultEvent {
    std::string   fault_type;
    FaultSeverity severity;
    std::string   source_ecu;
    double        timestamp;
    std::string   description;
    double        current_value;
    double        expected_min;
    double        expected_max;
    std::string   recommended_action;

    std::string toJson() const;
};

/**
 * @brief Rule-based fault detector.
 *
 * Evaluates the current VehicleState and DigitalTwin sync status
 * against a set of configured rules. Each rule maps an observable
 * condition to a FaultEvent with a severity level.
 *
 * Rules are transparent (explicit if/then logic) — no ML or
 * black-box detection. This is deliberate: in safety-critical
 * automotive software, detection logic must be auditable.
 *
 * Detection rules:
 *  1. ENGINE_OVERHEAT       — temperature > threshold
 *  2. ENGINE_RPM_HIGH       — rpm > threshold
 *  3. BRAKE_FAILURE         — brake active but pressure low
 *  4. BATTERY_LOW           — battery % < threshold
 *  5. BATTERY_TEMP_HIGH     — battery temp > threshold
 *  6. STEERING_FAULT        — angle > safe limit
 *  7. ECU_TIMEOUT           — no messages from ECU for > timeout
 *  8. SPEED_RPM_MISMATCH    — speed/RPM ratio inconsistent
 *
 * Each detected event is stored in a rolling buffer and accessible
 * via the API.
 */
class FaultDetector {
public:
    FaultDetector(const VehicleModel& model,
                  const DigitalTwin& twin,
                  const GatewayECU& gateway);

    /// Run all detection rules against current state. Returns new events.
    std::vector<FaultEvent> evaluate();

    /// All fault events (most recent first, up to max_events)
    std::deque<FaultEvent> getAllEvents(int max = 100) const;

    /// Active (unresolved) fault events
    std::vector<FaultEvent> getActiveEvents() const;

    // --- Configurable thresholds ---
    void setEngineOverheatWarning(double c)  { engine_temp_warning_  = c; }
    void setEngineOverheatCritical(double c) { engine_temp_critical_ = c; }
    void setRPMWarning(double rpm)           { rpm_warning_          = rpm; }
    void setRPMCritical(double rpm)          { rpm_critical_         = rpm; }
    void setBatteryWarning(double pct)       { battery_warning_      = pct; }
    void setBatteryCritical(double pct)      { battery_critical_     = pct; }
    void setECUTimeoutMs(int ms)             { ecu_timeout_ms_       = ms; }

    uint64_t getTotalDetected() const { return total_detected_.load(); }

private:
    FaultEvent makeEvent(const std::string& type,
                         FaultSeverity severity,
                         const std::string& ecu,
                         const std::string& desc,
                         double current,
                         double exp_min,
                         double exp_max,
                         const std::string& action) const;

    const VehicleModel& model_;
    const DigitalTwin&  twin_;
    const GatewayECU&   gateway_;

    mutable std::mutex events_mutex_;
    std::deque<FaultEvent> events_;

    // Active fault flags (to avoid duplicate events flooding)
    mutable std::mutex active_mutex_;
    std::unordered_map<std::string, bool> active_flags_;

    std::atomic<uint64_t> total_detected_{0};

    // Configurable thresholds (defaults match fault_config.json)
    double engine_temp_warning_{110.0};
    double engine_temp_critical_{130.0};
    double rpm_warning_{6000.0};
    double rpm_critical_{7000.0};
    double battery_warning_{20.0};
    double battery_critical_{10.0};
    double battery_temp_warning_{55.0};
    double battery_temp_critical_{65.0};
    double steering_safe_{480.0};
    double brake_pressure_min_{5.0};    ///< bar — below this with brake active = fault
    int    ecu_timeout_ms_{2000};
    double speed_rpm_ratio_max_{200.0}; ///< max RPM per km/h before mismatch

    static constexpr int EVENTS_MAX = 1000;
};

} // namespace vecu
