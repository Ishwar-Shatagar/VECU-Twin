#pragma once
#include "CANFrame.hpp"
#include "ThreadSafeQueue.hpp"
#include <functional>
#include <vector>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>

namespace vecu {

/// Callback type invoked when a CAN frame is delivered to a subscriber
using CANSubscriber = std::function<void(const CANFrame&)>;

/**
 * @brief Software CAN Bus abstraction.
 *
 * Models the logical behavior of an automotive CAN bus:
 *  - Any ECU can publish (transmit) a frame.
 *  - All ECUs that subscribed receive every frame (broadcast).
 *  - Arbitration: when multiple frames are queued simultaneously,
 *    the frame with the LOWEST CAN ID is delivered first.
 *    This mirrors physical CAN arbitration where the dominant bit
 *    (logic 0) wins — numerically lower IDs win arbitration.
 *
 * IMPORTANT: This is a SOFTWARE SIMULATION.
 * Physical CAN arbitration happens at the electrical bit level.
 * Here we model the outcome: lower CAN ID = higher priority.
 */
class CANBus {
public:
    explicit CANBus(const std::string& name = "MainBus");
    ~CANBus();

    // Non-copyable
    CANBus(const CANBus&) = delete;
    CANBus& operator=(const CANBus&) = delete;

    /// Subscribe a named participant. Callback invoked on every delivered frame.
    void subscribe(const std::string& participant_name, CANSubscriber callback);

    /**
     * @brief Publish a CAN frame onto the bus.
     *
     * Thread-safe. The frame is enqueued and dispatched by the
     * internal processing loop respecting CAN arbitration order.
     */
    void publish(const CANFrame& frame);

    /// Start the bus processing thread
    void start();

    /// Stop the bus (drains queue, then stops thread)
    void stop();

    // --- Statistics ---
    uint64_t getTotalFramesPublished() const { return total_published_.load(); }
    uint64_t getTotalFramesDelivered() const { return total_delivered_.load(); }
    double   getMessagesPerSecond() const;
    std::string getName() const { return name_; }

private:
    void processingLoop();

    std::string name_;

    /// Incoming frames waiting to be dispatched (MPSC)
    ThreadSafeQueue<CANFrame> incoming_;

    /// Priority queue sorted by CAN ID (lowest first = highest priority)
    struct PriorityCompare {
        bool operator()(const CANFrame& a, const CANFrame& b) const {
            return a.can_id > b.can_id; // min-heap: lower ID = higher priority
        }
    };

    /// Registered subscribers (name → callback)
    mutable std::mutex subscribers_mutex_;
    std::vector<std::pair<std::string, CANSubscriber>> subscribers_;

    std::thread processing_thread_;
    std::atomic<bool> running_{false};

    std::atomic<uint64_t> total_published_{0};
    std::atomic<uint64_t> total_delivered_{0};

    mutable std::mutex stats_mutex_;
    std::chrono::steady_clock::time_point start_time_;
};

} // namespace vecu
