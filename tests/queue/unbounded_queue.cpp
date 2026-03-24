#include "queue/unbounded_queue.hpp"
#include "gtest/gtest.h"
#include <chrono>
#include <cstdint>
#include <ranges>
#include <thread>
#include <vector>

namespace dispatcher::queue {

TEST(UnboundedQueueTest, Constructor) {
    UnboundedQueue queue{};
    EXPECT_EQ(0u, queue.size());
}

TEST(UnboundedQueueTest, TryPopEmptyQueue) {
    UnboundedQueue queue{};
    auto result = queue.try_pop();
    EXPECT_FALSE(result.has_value());
}

TEST(UnboundedQueueTest, PushAndTryPopSingleTask) {
    UnboundedQueue queue{};
    std::int32_t value{};
    auto task = [&value]() { value = 42; };

    queue.push(task);
    auto result{queue.try_pop()};

    ASSERT_TRUE(result.has_value());
    result.value()();
    EXPECT_EQ(42, value);
}

TEST(UnboundedQueueTest, AddManyTasks) {
    UnboundedQueue queue{};
    std::int32_t counter{};

    const int num_tasks{100};
    for (int i : std::views::iota(0, num_tasks)) {
        queue.push([&counter]() { ++counter; });
    }

    EXPECT_EQ(static_cast<std::size_t>(num_tasks), queue.size());

    for (int i : std::views::iota(0, num_tasks)) {
        auto result{queue.try_pop()};
        ASSERT_TRUE(result.has_value());
        result.value()();
    }

    EXPECT_EQ(num_tasks, counter);
    EXPECT_EQ(0u, queue.size());
}

TEST(UnboundedQueueTest, MultiThreadedPushPop) {
    const std::int32_t num_threads{4};
    const std::int32_t tasks_per_thread{25};
    UnboundedQueue queue{};

    std::vector<std::jthread> threads{};
    std::atomic<std::int32_t> total_tasks_executed{};
    std::atomic<bool> producers_finished{};

    for (std::int32_t i : std::views::iota(0LL, num_threads)) {
        threads.emplace_back([&queue, i, tasks_per_thread]() {
            for (std::int32_t j : std::views::iota(0LL, tasks_per_thread)) {
                auto task = [id = i * 1000 + j]() {};
                queue.push(std::move(task));
            }
        });
    }

    producers_finished.store(true);

    for (std::int32_t i : std::views::iota(0LL, num_threads)) {
        threads.emplace_back([&queue, &total_tasks_executed, &producers_finished]() {
            while (true) {
                auto task = queue.try_pop();
                if (task.has_value()) {
                    task.value()();
                    ++total_tasks_executed;
                } else {
                    if (producers_finished.load()) {
                        if (queue.size() == 0)
                            break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(num_threads * tasks_per_thread, total_tasks_executed.load());
}

TEST(UnboundedQueueTest, DestructorCleanup) {
    bool task_executed{false};

    {
        UnboundedQueue queue{};
        queue.push([&task_executed]() { task_executed = true; });
    }

    EXPECT_FALSE(task_executed);
}

}  // namespace dispatcher::queue
