#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <trantor/utils/TaskQueue.h>

namespace nodepulse::utils {

/**
 * @brief Thread-safe bounded worker task queue conforming to trantor::TaskQueue.
 *
 * Implements explicit finite capacity with race-safe admission, deterministic
 * saturation detection, and guaranteed deadlock-free shutdown.
 */
class BoundedTaskQueue : public trantor::TaskQueue {
  public:
    static constexpr size_t kDefaultMaxQueueSize = 64;

    explicit BoundedTaskQueue(size_t thread_num = 2, size_t max_queue_size = kDefaultMaxQueueSize,
                              std::string name = "bounded_worker");
    ~BoundedTaskQueue() override;

    // Disallow copy/move
    BoundedTaskQueue(const BoundedTaskQueue&) = delete;
    BoundedTaskQueue& operator=(const BoundedTaskQueue&) = delete;

    /**
     * @brief Attempt to enqueue a task.
     * @param task Callable to execute on worker thread.
     * @return true if enqueued; false if queue is full or stopping.
     */
    bool tryRunTaskInQueue(std::function<void()> task);

    /**
     * @brief trantor::TaskQueue interface: enqueue task if capacity permits.
     */
    void runTaskInQueue(const std::function<void()>& task) override;
    void runTaskInQueue(std::function<void()>&& task) override;

    [[nodiscard]] std::string getName() const override;
    [[nodiscard]] size_t getTaskCount() const;
    [[nodiscard]] size_t getCapacity() const noexcept;
    [[nodiscard]] bool isFull() const;

    void stop();

  private:
    void worker_loop();

    std::string name_;
    size_t max_queue_size_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<std::function<void()>> task_queue_;
    std::vector<std::thread> threads_;
    bool stop_{false};
};

}  // namespace nodepulse::utils
