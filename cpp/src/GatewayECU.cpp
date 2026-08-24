#include "GatewayECU.hpp"
#include "Logger.hpp"
#include "CANFrame.hpp"
#include <sstream>
#include <algorithm>

namespace vecu {

GatewayECU::GatewayECU(CANBus& bus, int update_hz, int timeout_ms)
    : VirtualECU("GATEWAY_ECU", 5, "0x50", update_hz, bus)
    , timeout_ms_(timeout_ms)
{}

void GatewayECU::receiveMessage(const CANFrame& frame) {
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto& h = ecu_health_[frame.source_ecu];
    h.online = true;
    h.message_count++;
    h.last_seen_s = frame.timestamp;
    h.timed_out = false;
    total_forwarded_.fetch_add(1);
}

std::unordered_map<std::string, GatewayECU::ECUHealth> GatewayECU::getECUHealthMap() const {
    std::lock_guard<std::mutex> lock(health_mutex_);
    return ecu_health_;
}

bool GatewayECU::isECUOnline(const std::string& ecu_name) const {
    std::lock_guard<std::mutex> lock(health_mutex_);
    auto it = ecu_health_.find(ecu_name);
    if (it == ecu_health_.end()) return false;
    return it->second.online && !it->second.timed_out;
}

void GatewayECU::update() {
    // Check for ECU timeouts
    double now = CANFrame::now();
    double timeout_s = timeout_ms_ / 1000.0;

    std::lock_guard<std::mutex> lock(health_mutex_);
    for (auto& [ecu_name, health] : ecu_health_) {
        if (health.online && (now - health.last_seen_s) > timeout_s) {
            if (!health.timed_out) {
                health.timed_out = true;
                LOG_WARN("GATEWAY_ECU", "ECU timeout detected: " + ecu_name);
            }
        }
    }
}

void GatewayECU::generateAndPublish() {
    // 0x501 GATEWAY_STATUS — ECU health flags
    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        std::array<uint8_t, 8> data{};
        // Bytes 0-4: health flags for known ECUs
        const std::vector<std::string> monitored = {
            "ENGINE_ECU", "BRAKE_ECU", "BATTERY_ECU", "STEERING_ECU"
        };
        for (size_t i = 0; i < monitored.size() && i < 4; ++i) {
            auto it = ecu_health_.find(monitored[i]);
            if (it != ecu_health_.end()) {
                data[i] = (it->second.online && !it->second.timed_out) ? 1 : 0;
            }
        }
        CANFrame frame(0x501, 8, data, name_, "GATEWAY_STATUS");
        publish(frame);
    }

    // 0x502 GATEWAY_STATS — total forwarded count
    {
        std::array<uint8_t, 8> data{};
        uint32_t count = static_cast<uint32_t>(total_forwarded_.load());
        data[0] = (count >> 24) & 0xFF;
        data[1] = (count >> 16) & 0xFF;
        data[2] = (count >>  8) & 0xFF;
        data[3] =  count        & 0xFF;
        CANFrame frame(0x502, 8, data, name_, "GATEWAY_STATS");
        publish(frame);
    }
}

} // namespace vecu
