#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <chrono>

namespace vecu {

/**
 * @brief Software-level CAN frame representation.
 *
 * This is a SOFTWARE SIMULATION of the CAN frame structure.
 * It does NOT reproduce physical CAN electrical signaling or timing.
 * It models the logical structure: ID, DLC, payload, and metadata.
 *
 * In real CAN (ISO 11898), frames are transmitted as differential
 * bit-serial signals on a two-wire bus. Here we pass C++ structs
 * through thread-safe in-process queues.
 */
struct CANFrame {
    double      timestamp;                  ///< Unix timestamp (seconds)
    uint32_t    can_id;                     ///< 11-bit or 29-bit CAN identifier
    uint8_t     dlc;                        ///< Data Length Code (0–8)
    std::array<uint8_t, 8> data{};         ///< Payload bytes
    std::string source_ecu;                 ///< Name of the originating ECU
    std::string message_name;              ///< Human-readable message label

    /// Construct with all fields
    CANFrame(uint32_t id,
             uint8_t dlc_val,
             const std::array<uint8_t, 8>& payload,
             const std::string& source,
             const std::string& msg_name);

    /// Default constructor (creates empty/invalid frame)
    CANFrame() = default;

    /// Returns current wall-clock time as a double (seconds since epoch)
    static double now();

    /// Encode a uint16 into bytes [offset] and [offset+1] (big-endian)
    static void encodeUInt16(std::array<uint8_t, 8>& data, int offset, uint16_t value);

    /// Encode a int16 into bytes [offset] and [offset+1] (big-endian)
    static void encodeInt16(std::array<uint8_t, 8>& data, int offset, int16_t value);

    /// Decode a uint16 from bytes [offset] and [offset+1] (big-endian)
    static uint16_t decodeUInt16(const std::array<uint8_t, 8>& data, int offset);

    /// Decode an int16 from bytes [offset] and [offset+1] (big-endian)
    static int16_t decodeInt16(const std::array<uint8_t, 8>& data, int offset);

    /// Serialize to a compact JSON string (for logging and IPC)
    std::string toJson() const;

    /// True if this frame has a lower CAN ID than other (higher arbitration priority)
    bool hasHigherPriorityThan(const CANFrame& other) const;
};

} // namespace vecu
