#include <utility>

#include <nodepulse/utils/bounded_task_queue.hpp>

namespace nodepulse::utils {

BoundedTaskQueue::BoundedTaskQueue(size_t thread_num, size_t max_queue_size, std::string name)
    : name_(std::move(name)), max_queue_size_(max_queue_size) {
    threads_.reserve(thread_num);
    for (size_t i = 0; i < thread_num; ++i) {
        threads_.emplace_back([this] { worker_loop(); });
    }
}

BoundedTaskQueue::~BoundedTaskQueue() {
    stop();
}

void BoundedTaskQueue::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_) {
            return;
        }
        stop_ = true;
    }
    cond_.notify_all();
    for (auto& t : threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
}

bool BoundedTaskQueue::tryRunTaskInQueue(std::function<void()> task) {
    if (!task) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_ || task_queue_.size() >= max_queue_size_) {
            return false;
        }
        task_queue_.push(std::move(task));
    }
    cond_.notify_one();
    return true;
}

void BoundedTaskQueue::runTaskInQueue(const std::function<void()>& task) {
    tryRunTaskInQueue(task);
}

void BoundedTaskQueue::runTaskInQueue(std::function<void()>&& task) {
    tryRunTaskInQueue(std::move(task));
}

std::string BoundedTaskQueue::getName() const {
    return name_;
}

size_t BoundedTaskQueue::getTaskCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_queue_.size();
}

size_t BoundedTaskQueue::getCapacity() const noexcept {
    return max_queue_size_;
}

bool BoundedTaskQueue::isFull() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return task_queue_.size() >= max_queue_size_;
}

void BoundedTaskQueue::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cond_.wait(lock, [this] { return stop_ || !task_queue_.empty(); });
            if (stop_ && task_queue_.empty()) {
                return;
            }
            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
            }
        }
        if (task) {
            try {
                task();
            } catch (...) {
                // Task exceptions are absorbed to preserve worker thread lifetime
            }
        }
    }
}

}  // namespace nodepulse::utils
