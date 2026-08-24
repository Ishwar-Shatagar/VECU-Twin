#pragma once
#include "VirtualECU.hpp"
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace vecu {

/**
 * @brief Virtual Gateway ECU.
 *
 * In real vehicles, a Gateway ECU bridges different CAN networks
 * (e.g., powertrain CAN, chassis CAN, comfort CAN) and filters/translates
 * messages between them. Here it demonstrates the concept in software.
 *
 * Responsibilities:
 *  - Receives all CAN frames from the shared bus.
 *  - Monitors last-seen time per source ECU.
 *  - Tracks per-ECU message counts and message rates.
 *  - Detects ECU communication timeouts.
 *  - Reports network health via GATEWAY_STATUS (0x501) and
 *    GATEWAY_STATS (0x502) CAN messages.
 *
 * Communication timeout detection:
 *  If no message is received from an ECU within `timeout_ms_`,
 *  that ECU is flagged as TIMEOUT in the health report.
 */
class GatewayECU : public VirtualECU {
public:
    GatewayECU(CANBus& bus, int update_hz = 10, int timeout_ms = 2000);
    ~GatewayECU() override = default;

    void receiveMessage(const CANFrame& frame) override;

    // --- Network health ---
    struct ECUHealth {
        bool   online{false};
        uint64_t message_count{0};
        double last_seen_s{0.0};
        bool   timed_out{false};
    };

    std::unordered_map<std::string, ECUHealth> getECUHealthMap() const;
    bool isECUOnline(const std::string& ecu_name) const;
    uint64_t getTotalForwarded() const { return total_forwarded_.load(); }

protected:
    void update() override;
    void generateAndPublish() override;

private:
    int timeout_ms_;

    mutable std::mutex health_mutex_;
    std::unordered_map<std::string, ECUHealth> ecu_health_;

    std::atomic<uint64_t> total_forwarded_{0};
};

} // namespace vecu
