#include "queue/bounded_queue.hpp"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <vector>

namespace dispatcher::queue {

TEST(BoundedQueueTest, ConstructorAndCapacity) {
    BoundedQueue queue(3);
    EXPECT_EQ(3, queue.capacity());
}

TEST(BoundedQueueTest, TryPopEmptyQueue) {
    BoundedQueue queue(5);
    auto result{queue.try_pop()};
    EXPECT_FALSE(result.has_value());
}

TEST(BoundedQueueTest, PushAndTryPopSingleTask) {
    BoundedQueue queue(5);
    std::int32_t value{};
    auto task = [&value]() { value = 42; };

    queue.push(task);
    auto result{queue.try_pop()};

    ASSERT_TRUE(result.has_value());
    result.value()();
    EXPECT_EQ(42, value);
}

TEST(BoundedQueueTest, FillToCapacity) {
    BoundedQueue queue(2);
    std::int32_t counter{};

    auto task1 = [&counter]() { ++counter; };
    auto task2 = [&counter]() { ++counter; };

    queue.push(task1);
    queue.push(task2);

    EXPECT_EQ(2, queue.size());

    auto result1{queue.try_pop()};
    auto result2{queue.try_pop()};

    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());

    result1.value()();
    result2.value()();

    EXPECT_EQ(2, counter);
}

TEST(BoundedQueueTest, PushToFullQueueBlocks) {
    BoundedQueue queue(1);
    std::atomic<bool> task_executed{false};

    queue.push([&task_executed]() { task_executed = true; });

    std::jthread blocker([&queue]() {
        auto task = []() {};
        queue.push(task);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto popped{queue.try_pop()};
    ASSERT_TRUE(popped.has_value());
    popped.value()();
    blocker.join();
    EXPECT_TRUE(task_executed);
}

TEST(BoundedQueueTest, MultiThreadedPushPop) {
    const std::int32_t num_threads{4};
    const std::int32_t tasks_per_thread{10};
    BoundedQueue queue(num_threads * 2);

    std::vector<std::jthread> threads;
    std::atomic<std::int32_t> total_tasks_executed{0};

    for (auto i : std::views::iota(0, num_threads)) {
        threads.emplace_back([&queue, &total_tasks_executed, &tasks_per_thread, i]() {
            for (auto j : std::views::iota(0, tasks_per_thread)) {
                auto task = [&total_tasks_executed, id = i * 1000 + j]() { ++total_tasks_executed; };
                queue.push(std::move(task));
            }
        });
    }

    for (auto i : std::views::iota(0, num_threads)) {
        threads.emplace_back([&queue, &total_tasks_executed]() {
            while (total_tasks_executed < num_threads * tasks_per_thread) {
                auto task = queue.try_pop();
                task.and_then([](auto &value) {
                    value();
                    return std::optional<std::function<void()>>{};
                });

                std::this_thread::yield();
            }
        });
    }

    for (auto &t : threads)
        t.join();

    EXPECT_EQ(num_threads * tasks_per_thread, total_tasks_executed);
}

TEST(BoundedQueueTest, NotificationCorrectness) {
    BoundedQueue queue(1);

    queue.push([] {});

    std::jthread waiting_thread([&queue]() { queue.push([] {}); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto popped{queue.try_pop()};
    ASSERT_TRUE(popped.has_value());
    waiting_thread.join();
    EXPECT_EQ(1, queue.size());
}

TEST(BoundedQueueTest, EdgeCapacityValues) {
    BoundedQueue small_queue(1);
    small_queue.push([] {});
    auto result{small_queue.try_pop()};
    ASSERT_TRUE(result.has_value());
}

TEST(BoundedQueueTest, EdgeZeroCapacityValues) {
    EXPECT_THROW({ BoundedQueue{0}; }, std::invalid_argument);
}

}  // namespace dispatcher::queue
