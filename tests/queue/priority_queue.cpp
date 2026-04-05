#include "queue/priority_queue.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <optional>
#include <thread>
#include <vector>

namespace dispatcher::queue {

class PriorityQueueTest : public ::testing::Test {
protected:
    void SetUp() override {
        QueueOptions high_options{true, 100};
        QueueOptions normal_options{false, std::nullopt};

        std::map<TaskPriority, QueueOptions> config{{TaskPriority::High, high_options},
                                                    {TaskPriority::Normal, normal_options}};

        queue_ = std::make_unique<PriorityQueue>(config);
    }

    std::unique_ptr<PriorityQueue> queue_;
};

TEST_F(PriorityQueueTest, FailInitHighPriority) {
    QueueOptions fail_high_options{false, 10};
    std::map<TaskPriority, QueueOptions> fail_high_config{{TaskPriority::High, fail_high_options}};
    EXPECT_THROW(std::make_unique<PriorityQueue>(fail_high_config), std::invalid_argument);
}

TEST_F(PriorityQueueTest, FailInitNormalPriority) {
    QueueOptions fail_normal_options{false, 10};
    std::map<TaskPriority, QueueOptions> fail_normal_config{{TaskPriority::Normal, fail_normal_options}};
    EXPECT_THROW(std::make_unique<PriorityQueue>(fail_normal_config), std::invalid_argument);
}

TEST_F(PriorityQueueTest, PushAndPopHighPriority) {
    std::int32_t counter{0};

    queue_->push(TaskPriority::High, [&counter]() { counter += 1; });

    auto task{queue_->pop()};
    ASSERT_TRUE(task.has_value());
    task.value()();

    EXPECT_EQ(counter, 1);
}

TEST_F(PriorityQueueTest, HighPriorityBeforeNormal) {
    std::int32_t execution_order{};
    std::vector<std::int32_t> order_log;

    queue_->push(TaskPriority::Normal, [&execution_order, &order_log]() {
        auto current_order{++execution_order};
        order_log.push_back(current_order);
    });

    queue_->push(TaskPriority::High, [&execution_order, &order_log]() {
        auto current_order{++execution_order};
        order_log.push_back(current_order);
    });

    auto task1{queue_->pop()};
    auto task2{queue_->pop()};

    ASSERT_TRUE(task1.has_value());
    ASSERT_TRUE(task2.has_value());

    task1.value()();
    task2.value()();

    ASSERT_EQ(order_log.size(), 2);
    EXPECT_EQ(order_log[0], 1);
    EXPECT_EQ(order_log[1], 2);
}

TEST_F(PriorityQueueTest, PopBlocksWhenEmpty) {
    std::atomic<bool> blocker_started{};
    std::atomic<bool> popped{};
    std::condition_variable cv;
    std::mutex mtx;

    std::thread blocker([this, &popped, &blocker_started, &cv, &mtx]() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            blocker_started = true;
            cv.notify_one();
        }

        auto result{queue_->pop()};
        result.and_then([&popped](auto &value) {
            value();
            popped = true;
            return std::optional<std::function<void()>>{};
        });
    });

    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&blocker_started]() { return blocker_started.load(); });
    }

    auto start{std::chrono::steady_clock::now()};
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    auto elapsed{std::chrono::steady_clock::now() - start};
    EXPECT_GE(elapsed, std::chrono::milliseconds(95));

    std::int32_t task_value{};

    queue_->push(TaskPriority::High, [&task_value]() { task_value = 42; });

    blocker.join();

    EXPECT_TRUE(popped.load());
    EXPECT_EQ(task_value, 42);
}

TEST_F(PriorityQueueTest, ShutdownReturnsNullopt) {
    queue_->shutdown();
    auto result{queue_->pop()};
    EXPECT_FALSE(result.has_value());
}

TEST_F(PriorityQueueTest, PopAfterShutdownReturnsNullopt) {
    std::int32_t task_executed{};
    queue_->push(TaskPriority::High, [&task_executed]() { task_executed = 1; });
    auto result1{queue_->pop()};
    result1.value()();
    queue_->shutdown();

    auto result2{queue_->pop()};

    EXPECT_TRUE(result1.has_value());
    EXPECT_FALSE(result2.has_value());
    EXPECT_EQ(task_executed, 1);
}

TEST_F(PriorityQueueTest, MultipleThreads) {
    const std::int32_t num_tasks{100};
    std::atomic<std::int32_t> high_counter{};
    std::atomic<std::int32_t> normal_counter{};

    std::vector<std::thread> producers;
    for (auto i : std::views::iota(0, 5)) {
        producers.emplace_back([this, &high_counter, &normal_counter, i, num_tasks]() {
            for (auto j : std::views::iota(0, num_tasks / 5)) {
                if (j % 2 == 0)
                    queue_->push(TaskPriority::High, [&high_counter]() { high_counter++; });
                else
                    queue_->push(TaskPriority::Normal, [&normal_counter]() { normal_counter++; });
            }
        });
    }

    std::atomic<std::int32_t> total_popped{};
    std::thread consumer([this, &total_popped, num_tasks]() {
        while (total_popped.load() < num_tasks) {
            auto task = queue_->pop();
            task.and_then([this, &total_popped](auto &value) {
                value();
                ++total_popped;
                return std::optional<std::function<void()>>{};
            });
        }
    });

    queue_->shutdown();
    for (auto &t : producers)
        t.join();

    consumer.join();

    EXPECT_EQ(high_counter + normal_counter, num_tasks);
}

TEST_F(PriorityQueueTest, DestructorCallsShutdown) {
    std::unique_ptr<PriorityQueue> temp_queue;
    {
        QueueOptions options{false, std::nullopt};
        std::map<TaskPriority, QueueOptions> config{{TaskPriority::High, options}};
        temp_queue = std::make_unique<PriorityQueue>(config);

        std::atomic<bool> pop_started{};
        std::condition_variable cv;
        std::mutex mtx;

        std::thread waiter([&temp_queue, &pop_started, &cv, &mtx]() {
            {
                std::lock_guard<std::mutex> lock(mtx);
                pop_started = true;
                cv.notify_one();
            }

            auto result = temp_queue->pop();
            EXPECT_FALSE(result.has_value());
        });

        {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [&pop_started]() { return pop_started.load(); });
        }

        temp_queue.reset();
        waiter.join();
    }
}

TEST_F(PriorityQueueTest, PushInvalidPriorityTask) {
    QueueOptions options{false, std::nullopt};
    std::map<TaskPriority, QueueOptions> config{{TaskPriority::High, options}};
    std::unique_ptr<PriorityQueue> temp_queue;
    temp_queue = std::make_unique<PriorityQueue>(config);
    EXPECT_THROW(temp_queue->push(TaskPriority::Normal, []() {}), std::invalid_argument);
}
}  // namespace dispatcher::queue