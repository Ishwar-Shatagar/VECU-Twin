#include "VirtualECU.hpp"
#include "Logger.hpp"
#include <chrono>
#include <thread>

namespace vecu {

std::string ecuStatusToString(ECUStatus status) {
    switch (status) {
        case ECUStatus::OFFLINE:  return "OFFLINE";
        case ECUStatus::STARTING: return "STARTING";
        case ECUStatus::ONLINE:   return "ONLINE";
        case ECUStatus::FAULT:    return "FAULT";
        case ECUStatus::STOPPED:  return "STOPPED";
    }
    return "UNKNOWN";
}

VirtualECU::VirtualECU(std::string name,
                        uint32_t ecu_id,
                        std::string can_id_prefix,
                        int update_frequency_hz,
                        CANBus& bus)
    : name_(std::move(name))
    , ecu_id_(ecu_id)
    , can_id_prefix_(std::move(can_id_prefix))
    , update_frequency_hz_(update_frequency_hz)
    , bus_(bus)
    , start_time_(std::chrono::steady_clock::now())
    , last_message_time_(std::chrono::steady_clock::now())
{}

VirtualECU::~VirtualECU() {
    stop();
}

void VirtualECU::start() {
    if (running_.exchange(true)) return;
    status_.store(ECUStatus::STARTING);
    LOG_INFO(name_, "ECU starting at " + std::to_string(update_frequency_hz_) + " Hz");
    start_time_ = std::chrono::steady_clock::now();
    thread_ = std::thread(&VirtualECU::threadLoop, this);
    status_.store(ECUStatus::ONLINE);
    LOG_INFO(name_, "ECU online");
}

void VirtualECU::stop() {
    if (!running_.exchange(false)) return;
    if (thread_.joinable()) {
        thread_.join();
    }
    status_.store(ECUStatus::STOPPED);
    LOG_INFO(name_, "ECU stopped");
}

void VirtualECU::receiveMessage(const CANFrame& /*frame*/) {
    // Default: no-op. Subclasses override if they process incoming messages.
}

/**
 * @brief ECU thread main loop.
 *
 * Runs at update_frequency_hz_, calling update() then generateAndPublish()
 * each cycle. Uses a sleep-based scheduler. A production system would use
 * OS timer callbacks or a RTOS task scheduler, but sleep is sufficient for
 * a software simulation demonstrating the concept.
 *
 * Safe shutdown: the running_ atomic is set to false by stop(), which causes
 * the loop to exit cleanly on the next iteration.
 */
void VirtualECU::threadLoop() {
    const auto period = std::chrono::microseconds(
        static_cast<long long>(1'000'000.0 / update_frequency_hz_));

    auto next_tick = std::chrono::steady_clock::now();

    while (running_.load()) {
        auto tick_start = std::chrono::steady_clock::now();

        // Advance simulation state
        update();

        // Generate and publish CAN frames (unless silenced by FaultEngine)
        if (!silenced_.load()) {
            int delay = message_delay_ms_.load();
            if (delay > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
            generateAndPublish();
        }

        // Update fault status
        if (fault_active_.load()) {
            status_.store(ECUStatus::FAULT);
            fault_count_.fetch_add(1);
        } else {
            status_.store(ECUStatus::ONLINE);
        }

        // Sleep until next tick (deadline-based, not sleep-from-now)
        next_tick += period;
        std::this_thread::sleep_until(next_tick);
    }
}

void VirtualECU::publish(CANFrame frame) {
    {
        std::lock_guard<std::mutex> lock(timing_mutex_);
        last_message_time_ = std::chrono::steady_clock::now();
    }
    message_count_.fetch_add(1);
    bus_.publish(frame);
}

double VirtualECU::getMessagesPerSecond() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - start_time_).count();
    if (elapsed < 0.001) return 0.0;
    return static_cast<double>(message_count_.load()) / elapsed;
}

double VirtualECU::getLastMessageTime() const {
    std::lock_guard<std::mutex> lock(timing_mutex_);
    return std::chrono::duration<double>(
        last_message_time_.time_since_epoch()).count();
}

} // namespace vecu
