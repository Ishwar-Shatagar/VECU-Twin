#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace vecu {

/**
 * @brief Thread-safe MPSC (multi-producer, single-consumer) queue.
 *
 * Uses a std::mutex and std::condition_variable to implement
 * the classic producer-consumer pattern. Multiple ECU threads
 * (producers) push CAN frames; the CAN bus processing thread
 * (consumer) pops them.
 *
 * Why needed: std::queue is NOT thread-safe. Without synchronization,
 * concurrent push/pop would cause data races and undefined behavior.
 */
template <typename T>
class ThreadSafeQueue {
public:
    ThreadSafeQueue() = default;

    // Non-copyable, non-movable (owns mutex)
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    /// Push an item. Notifies one waiting consumer.
    void push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * @brief Blocking pop. Waits until an item is available or stop is requested.
     * @param[out] item  Receives the popped item.
     * @return true if an item was returned; false if the queue was stopped.
     */
    bool pop(T& item) {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (stopped_ && queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    /**
     * @brief Non-blocking pop.
     * @return The item if available, or std::nullopt.
     */
    std::optional<T> tryPop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    /**
     * @brief Timed pop: waits at most `timeout_ms` milliseconds.
     * @return true if item received within timeout.
     */
    bool popFor(T& item, int timeout_ms) {
        std::unique_lock<std::mutex> lock(mutex_);
        bool notified = cv_.wait_for(lock,
            std::chrono::milliseconds(timeout_ms),
            [this] { return !queue_.empty() || stopped_; });
        if (!notified || (stopped_ && queue_.empty())) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    /// Signal all blocked pops to return false (shutdown).
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex      mutex_;
    std::condition_variable cv_;
    std::queue<T>           queue_;
    bool                    stopped_{false};
};

} // namespace vecu
