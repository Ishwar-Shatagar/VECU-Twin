#include "CANBus.hpp"
#include "Logger.hpp"
#include <thread>
#include <algorithm>
#include <queue>
#include <chrono>

namespace vecu {

CANBus::CANBus(const std::string& name)
    : name_(name)
    , start_time_(std::chrono::steady_clock::now())
{}

CANBus::~CANBus() {
    stop();
}

void CANBus::subscribe(const std::string& participant_name, CANSubscriber callback) {
    std::lock_guard<std::mutex> lock(subscribers_mutex_);
    subscribers_.emplace_back(participant_name, std::move(callback));
    LOG_INFO("CANBus", "Subscriber registered: " + participant_name);
}

void CANBus::publish(const CANFrame& frame) {
    if (!running_.load()) return;
    incoming_.push(frame);
    total_published_.fetch_add(1);
}

void CANBus::start() {
    if (running_.exchange(true)) return;  // already running
    start_time_ = std::chrono::steady_clock::now();
    processing_thread_ = std::thread(&CANBus::processingLoop, this);
    LOG_INFO("CANBus", "Bus '" + name_ + "' started");
}

void CANBus::stop() {
    if (!running_.exchange(false)) return;
    incoming_.stop();
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    LOG_INFO("CANBus", "Bus '" + name_ + "' stopped");
}

/**
 * @brief Main CAN bus processing loop.
 *
 * Pops frames from the incoming queue, applies priority ordering
 * (lower CAN ID = higher priority), and delivers to all subscribers.
 *
 * CAN arbitration simulation:
 *  We drain all available frames from the incoming queue, sort them
 *  by CAN ID (ascending), then deliver in priority order. This models
 *  the outcome of physical CAN arbitration without bit-level simulation.
 */
void CANBus::processingLoop() {
    while (running_.load()) {
        CANFrame frame;
        // Blocking pop — waits for a frame or stop signal
        if (!incoming_.pop(frame)) break;

        // Drain any additional frames already waiting (batch arbitration)
        std::vector<CANFrame> batch;
        batch.push_back(frame);

        std::optional<CANFrame> extra;
        while ((extra = incoming_.tryPop()).has_value()) {
            batch.push_back(extra.value());
        }

        // Sort batch by CAN ID ascending (lower = higher priority)
        std::sort(batch.begin(), batch.end(),
            [](const CANFrame& a, const CANFrame& b) {
                return a.can_id < b.can_id;
            });

        // Deliver each frame to all subscribers
        std::lock_guard<std::mutex> lock(subscribers_mutex_);
        for (const auto& f : batch) {
            for (const auto& [name, cb] : subscribers_) {
                try {
                    cb(f);
                } catch (const std::exception& e) {
                    LOG_WARN("CANBus", "Subscriber " + name + " threw: " + e.what());
                }
            }
            total_delivered_.fetch_add(1);
        }
    }
}

double CANBus::getMessagesPerSecond() const {
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    if (elapsed < 0.001) return 0.0;
    return static_cast<double>(total_delivered_.load()) / elapsed;
}

} // namespace vecu
