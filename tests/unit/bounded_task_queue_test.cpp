#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/utils/bounded_task_queue.hpp>

namespace nodepulse::utils {
namespace {

TEST(BoundedTaskQueueTest, BasicExecution) {
    auto queue = std::make_shared<BoundedTaskQueue>(2, 10, "test_basic");
    std::atomic<int> counter{0};

    std::promise<void> p1;
    std::promise<void> p2;

    EXPECT_TRUE(queue->tryRunTaskInQueue([&counter, &p1]() {
        counter.fetch_add(1);
        p1.set_value();
    }));

    EXPECT_TRUE(queue->tryRunTaskInQueue([&counter, &p2]() {
        counter.fetch_add(1);
        p2.set_value();
    }));

    p1.get_future().wait();
    p2.get_future().wait();

    EXPECT_EQ(counter.load(), 2);
    EXPECT_EQ(queue->getName(), "test_basic");
}

TEST(BoundedTaskQueueTest, CapacityEnforcement) {
    // 1 worker thread, max queue size 2
    auto queue = std::make_shared<BoundedTaskQueue>(1, 2, "test_capacity");

    std::promise<void> unblock_worker;
    auto unblock_worker_future = unblock_worker.get_future().share();
    std::promise<void> worker_running;

    // Task 1: blocks the 1 worker thread
    EXPECT_TRUE(queue->tryRunTaskInQueue([&worker_running, unblock_worker_future]() {
        worker_running.set_value();
        unblock_worker_future.wait();
    }));

    worker_running.get_future().wait();

    // Now the 1 worker thread is busy. The queue capacity is 2.
    // Task 2: queued (slot 1 of 2)
    EXPECT_TRUE(queue->tryRunTaskInQueue([]() {}));
    // Task 3: queued (slot 2 of 2)
    EXPECT_TRUE(queue->tryRunTaskInQueue([]() {}));

    // Task 4: queue is saturated -> must be rejected
    std::atomic<bool> rejected_executed{false};
    bool admitted =
        queue->tryRunTaskInQueue([&rejected_executed]() { rejected_executed.store(true); });
    EXPECT_FALSE(admitted);

    // Unblock the worker so all queued tasks can finish
    unblock_worker.set_value();
    queue->stop();

    EXPECT_FALSE(rejected_executed.load());
}

TEST(BoundedTaskQueueTest, RecoveryAfterSaturation) {
    // 1 worker thread, max queue size 1
    auto queue = std::make_shared<BoundedTaskQueue>(1, 1, "test_recovery");

    std::promise<void> unblock_first;
    auto unblock_first_future = unblock_first.get_future().share();
    std::promise<void> first_running;

    EXPECT_TRUE(queue->tryRunTaskInQueue([&first_running, unblock_first_future]() {
        first_running.set_value();
        unblock_first_future.wait();
    }));

    first_running.get_future().wait();

    // Queue slot is now full (1 of 1)
    EXPECT_TRUE(queue->tryRunTaskInQueue([]() {}));

    // Next must be rejected
    EXPECT_FALSE(queue->tryRunTaskInQueue([]() {}));

    // Unblock and wait for drain
    unblock_first.set_value();

    // Wait until queue is empty
    for (int i = 0; i < 50 && queue->getTaskCount() > 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Now admission must succeed again
    std::promise<void> new_task_done;
    EXPECT_TRUE(queue->tryRunTaskInQueue([&new_task_done]() { new_task_done.set_value(); }));
    new_task_done.get_future().wait();
}

TEST(BoundedTaskQueueTest, CleanShutdownWithoutTasks) {
    auto queue = std::make_shared<BoundedTaskQueue>(2, 10, "test_shutdown");
    EXPECT_NO_THROW(queue->stop());
    // Tasks submitted after stop should be rejected
    EXPECT_FALSE(queue->tryRunTaskInQueue([]() {}));
}

}  // namespace
}  // namespace nodepulse::utils
