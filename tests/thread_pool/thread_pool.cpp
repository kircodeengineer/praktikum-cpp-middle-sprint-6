#include "thread_pool/thread_pool.hpp"
#include "queue/priority_queue.hpp"
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <print>
#include <ranges>
#include <thread>
#include <vector>

namespace dispatcher::thread_pool {

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаём конфигурацию для приоритетной очереди
        std::map<dispatcher::TaskPriority, dispatcher::queue::QueueOptions> options;
        options[dispatcher::TaskPriority::High] = dispatcher::queue::QueueOptions{true, 100};
        options[dispatcher::TaskPriority::Normal] = dispatcher::queue::QueueOptions{false};

        queue_ = std::make_shared<dispatcher::queue::PriorityQueue>(options);
    }

    std::shared_ptr<dispatcher::queue::PriorityQueue> queue_;
};

TEST_F(ThreadPoolTest, CreationAndDestruction) {
    {
        ThreadPool pool(queue_, 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    EXPECT_TRUE(queue_ != nullptr);
}

TEST_F(ThreadPoolTest, SimpleTaskExecution) {
    ThreadPool pool(queue_, 2);

    std::atomic<std::int32_t> counter{};
    const std::int32_t num_tasks{10};

    for (std::int32_t i = 0; i < num_tasks; ++i) {
        queue_->push(dispatcher::TaskPriority::Normal, [&counter] {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            counter++;
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(counter.load(), num_tasks);
}

TEST_F(ThreadPoolTest, ThreadCount) {
    const std::size_t num_threads{4};
    ThreadPool pool(queue_, num_threads);

    EXPECT_EQ(pool.size(), num_threads);
}

TEST_F(ThreadPoolTest, PriorityExecution) {
    ThreadPool pool(queue_, 2);

    std::vector<int> execution_order;
    std::mutex order_mutex;

    const std::int32_t num_tasks{3};
    for (auto i : std::views::iota(0, num_tasks)) {
        queue_->push(dispatcher::TaskPriority::High, [&execution_order, &order_mutex, i] {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(100 + i);
        });
    }

    for (auto i : std::views::iota(0, num_tasks)) {
        queue_->push(dispatcher::TaskPriority::Normal, [&execution_order, &order_mutex, i] {
            std::lock_guard<std::mutex> lock(order_mutex);
            execution_order.push_back(i);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    ASSERT_GE(execution_order.size(), num_tasks * 2);
    for (auto i : std::views::iota(0, num_tasks)) {
        EXPECT_GE(execution_order[i], 100);
    }
}

TEST_F(ThreadPoolTest, DestructorDoesNotHang) {
    {
        ThreadPool pool(queue_, 2);

        for (auto i : std::views::iota(0, 5)) {
            queue_->push(dispatcher::TaskPriority::Normal, [i] {
                std::println("Задание №{} выполнено", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            });
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    EXPECT_TRUE(true);
}

TEST_F(ThreadPoolTest, TasksAfterShutdown) {
    ThreadPool pool(queue_, 2);

    std::atomic<std::int32_t> task_counter{0};

    const std::int32_t num_tasks{3};
    for (std::int32_t i = 0; i < num_tasks; ++i) {
        queue_->push(dispatcher::TaskPriority::Normal, [&task_counter] {
            task_counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    queue_->shutdown();

    bool task_executed{};
    queue_->push(dispatcher::TaskPriority::Normal, [&task_executed] { task_executed = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_FALSE(task_executed);
    EXPECT_GE(task_counter.load(), num_tasks);
}

TEST_F(ThreadPoolTest, LoadTest) {
    ThreadPool pool(queue_, 4);

    const std::int32_t total_tasks{50};
    std::atomic<std::int32_t> completed_tasks{};

    for (auto i : std::views::iota(0, total_tasks)) {
        auto priority{(i % 3 == 0) ? dispatcher::TaskPriority::High : dispatcher::TaskPriority::Normal};

        queue_->push(priority, [&completed_tasks] {
            completed_tasks++;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        });
    }

    auto start_time{std::chrono::steady_clock::now()};
    while (completed_tasks.load() < total_tasks) {
        if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(5)) {
            FAIL() << "Время ожидания вышло";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_EQ(completed_tasks.load(), total_tasks);
}

TEST_F(ThreadPoolTest, EmptyPoolDestruction) {
    {
        ThreadPool pool(queue_, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    EXPECT_TRUE(queue_ != nullptr);
}

}  // namespace dispatcher::thread_pool
