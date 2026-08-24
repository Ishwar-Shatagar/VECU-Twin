#pragma once
#include "CANFrame.hpp"
#include "CANBus.hpp"
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>

namespace vecu {

/// ECU operational status
enum class ECUStatus {
    OFFLINE,
    STARTING,
    ONLINE,
    FAULT,
    STOPPED
};

std::string ecuStatusToString(ECUStatus status);

/**
 * @brief Abstract base class for all Virtual ECUs.
 *
 * Provides the common ECU lifecycle (start/stop), CAN bus
 * integration, message publishing, and health tracking.
 * Concrete ECUs derive from this class and implement:
 *   - update()        : advance simulation state by one tick
 *   - generateMessage(): produce CAN frames from current state
 *
 * Each ECU runs in its own std::thread at a configured frequency.
 * The thread is cleanly shut down via the running_ atomic flag.
 */
class VirtualECU {
public:
    VirtualECU(std::string name,
               uint32_t ecu_id,
               std::string can_id_prefix,
               int update_frequency_hz,
               CANBus& bus);

    virtual ~VirtualECU();

    // Non-copyable
    VirtualECU(const VirtualECU&) = delete;
    VirtualECU& operator=(const VirtualECU&) = delete;

    /// Start the ECU thread
    virtual void start();

    /// Stop the ECU thread (blocks until thread exits)
    virtual void stop();

    /// Called by CANBus when a frame arrives (subscriber callback)
    virtual void receiveMessage(const CANFrame& frame);

    // --- State accessors ---
    std::string getName()    const { return name_; }
    uint32_t    getECUID()   const { return ecu_id_; }
    ECUStatus   getStatus()  const { return status_.load(); }
    std::string getStatusString() const { return ecuStatusToString(status_.load()); }
    uint64_t    getMessageCount()  const { return message_count_.load(); }
    double      getMessagesPerSecond() const;
    uint64_t    getFaultCount()    const { return fault_count_.load(); }
    double      getLastMessageTime() const;
    int         getUpdateFrequencyHz() const { return update_frequency_hz_; }

    /// True if ECU is actively running
    bool isRunning() const { return running_.load(); }

    /// Called externally to inject a fault flag
    void setFaultActive(bool active) { fault_active_.store(active); }
    bool isFaultActive() const { return fault_active_.load(); }

    /// Set a communication silence flag (used by FaultEngine)
    void setSilenced(bool silenced) { silenced_.store(silenced); }
    bool isSilenced() const { return silenced_.load(); }

    /// Set artificial message delay in ms (used by FaultEngine)
    void setMessageDelay(int delay_ms) { message_delay_ms_.store(delay_ms); }
    int getMessageDelay() const { return message_delay_ms_.load(); }

protected:
    /// Advance internal simulation state by one update tick.
    /// Called from the ECU thread at update_frequency_hz_.
    virtual void update() = 0;

    /// Produce and publish one or more CAN frames based on current state.
    virtual void generateAndPublish() = 0;

    /// Publish a frame onto the shared CAN bus
    void publish(CANFrame frame);

    // --- Shared members accessible to subclasses ---
    std::string  name_;
    uint32_t     ecu_id_;
    std::string  can_id_prefix_;
    int          update_frequency_hz_;
    CANBus&      bus_;

    std::atomic<ECUStatus>  status_{ECUStatus::OFFLINE};
    std::atomic<bool>       running_{false};
    std::atomic<bool>       fault_active_{false};
    std::atomic<bool>       silenced_{false};
    std::atomic<int>        message_delay_ms_{0};
    std::atomic<uint64_t>   message_count_{0};
    std::atomic<uint64_t>   fault_count_{0};

    mutable std::mutex      state_mutex_;

private:
    void threadLoop();

    std::thread thread_;

    mutable std::mutex        timing_mutex_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_message_time_;
};

} // namespace vecu
