#include "CANFrame.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace vecu {

CANFrame::CANFrame(uint32_t id,
                   uint8_t dlc_val,
                   const std::array<uint8_t, 8>& payload,
                   const std::string& source,
                   const std::string& msg_name)
    : can_id(id)
    , dlc(dlc_val)
    , data(payload)
    , source_ecu(source)
    , message_name(msg_name)
{
    timestamp = now();
}

double CANFrame::now() {
    using namespace std::chrono;
    auto tp = system_clock::now();
    return duration<double>(tp.time_since_epoch()).count();
}

void CANFrame::encodeUInt16(std::array<uint8_t, 8>& d, int offset, uint16_t value) {
    d[offset]     = static_cast<uint8_t>((value >> 8) & 0xFF);
    d[offset + 1] = static_cast<uint8_t>(value & 0xFF);
}

void CANFrame::encodeInt16(std::array<uint8_t, 8>& d, int offset, int16_t value) {
    encodeUInt16(d, offset, static_cast<uint16_t>(value));
}

uint16_t CANFrame::decodeUInt16(const std::array<uint8_t, 8>& d, int offset) {
    return static_cast<uint16_t>((d[offset] << 8) | d[offset + 1]);
}

int16_t CANFrame::decodeInt16(const std::array<uint8_t, 8>& d, int offset) {
    return static_cast<int16_t>(decodeUInt16(d, offset));
}

std::string CANFrame::toJson() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\"timestamp\":" << timestamp
       << ",\"can_id\":\"0x" << std::hex << std::uppercase << std::setw(3) << std::setfill('0') << can_id << "\""
       << std::dec
       << ",\"dlc\":" << static_cast<int>(dlc)
       << ",\"data\":[";
    for (int i = 0; i < 8; ++i) {
        ss << static_cast<int>(data[i]);
        if (i < 7) ss << ",";
    }
    ss << "]"
       << ",\"source\":\"" << source_ecu << "\""
       << ",\"message\":\"" << message_name << "\""
       << "}";
    return ss.str();
}

bool CANFrame::hasHigherPriorityThan(const CANFrame& other) const {
    // Lower CAN ID = higher priority (mirrors physical CAN arbitration outcome)
    return can_id < other.can_id;
}

} // namespace vecu
