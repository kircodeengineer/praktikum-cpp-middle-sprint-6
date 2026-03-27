#include "task_dispatcher.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <print>
#include <string>
#include <thread>
#include <vector>

namespace dispatcher {
using namespace std::chrono_literals;

TEST(TaskDispatcherTest, ScheduleTask) {
    TaskDispatcher dispatcher(1);

    bool task_executed{};
    dispatcher.Schedule(TaskPriority::High, [&task_executed]() { task_executed = true; });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(task_executed);
}

class TaskDispatcherPriorityTest : public ::testing::Test {
protected:
    void SetUp() override { dispatcher = std::make_unique<TaskDispatcher>(1); }

    void TearDown() override { dispatcher.reset(); }

    std::unique_ptr<TaskDispatcher> dispatcher;
};

TEST_F(TaskDispatcherPriorityTest, SequentialExecutionWithPriority) {
    std::vector<std::string> execution_order;
    constexpr int TASK_COUNT{5};

    for (auto i : std::views::iota(0, TASK_COUNT)) {
        dispatcher->Schedule(TaskPriority::Normal, [&execution_order, i]() {
            std::this_thread::sleep_for(10ms);
            execution_order.push_back("Normal_" + std::to_string(i));
        });

        dispatcher->Schedule(TaskPriority::High, [&execution_order, i]() {
            std::this_thread::sleep_for(5ms);
            execution_order.push_back("High_" + std::to_string(i));
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(TASK_COUNT * 15 + 100));

    ASSERT_EQ(execution_order.size(), TASK_COUNT * 2);

    auto all_tasks_count{static_cast<std::int32_t>(execution_order.size())};
    for (auto i : std::views::iota(0, all_tasks_count)) {
        if (i < TASK_COUNT) {
            EXPECT_TRUE(execution_order[i].find("High_") == 0)
                << "Задача " << i << " должна быть High приоритета, но была: " << execution_order[i];
        } else {
            EXPECT_TRUE(execution_order[i].find("Normal_") == 0)
                << "Задача " << i << " должна быть Normal приоритета, но была: " << execution_order[i];
        }
    }
}

TEST_F(TaskDispatcherPriorityTest, ImmediateHighPriorityExecution) {
    std::vector<std::string> execution_order;

    dispatcher->Schedule(TaskPriority::Normal, [&execution_order]() {
        std::this_thread::sleep_for(50ms);
        execution_order.push_back("Normal_first");
    });

    dispatcher->Schedule(TaskPriority::High, [&execution_order]() { execution_order.push_back("High_immediate"); });

    std::this_thread::sleep_for(100ms);

    ASSERT_EQ(execution_order.size(), 2);
    EXPECT_EQ(execution_order[0], "High_immediate");
    EXPECT_EQ(execution_order[1], "Normal_first");
}

class TaskDispatcherShutdownTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Инициализируем диспетчер с 2 потоками для параллельного выполнения
        dispatcher = std::make_unique<TaskDispatcher>(2);
    }

    void TearDown() override { dispatcher.reset(); }

    std::unique_ptr<TaskDispatcher> dispatcher;
};

TEST_F(TaskDispatcherShutdownTest, AllTasksExecuteBeforeShutdown) {
    constexpr int TASK_COUNT = 10;
    std::atomic<int> completed_tasks{0};
    std::mutex cv_mutex;
    std::condition_variable cv;
    bool all_tasks_completed = false;

    for (int i = 0; i < TASK_COUNT; ++i) {
        TaskPriority priority = (i % 2 == 0) ? TaskPriority::High : TaskPriority::Normal;
        auto sleep_ms = (i % 5 + 1) * 20;  // 20–100 мс

        dispatcher->Schedule(priority, [&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
            int current = completed_tasks.fetch_add(1, std::memory_order_relaxed);
            if (current + 1 == TASK_COUNT) {
                std::lock_guard<std::mutex> lock(cv_mutex);
                all_tasks_completed = true;
                cv.notify_all();
            }
        });
    }
    dispatcher.reset();

    const auto timeout = 10s;
    std::unique_lock<std::mutex> lock(cv_mutex);
    if (!cv.wait_for(lock, timeout, [&all_tasks_completed] { return all_tasks_completed; })) {
        FAIL() << "Timeout: не все задачи завершились за " << timeout / 1s
               << " секунд. Выполнено: " << completed_tasks.load(std::memory_order_relaxed) << " из " << TASK_COUNT;
    }

    EXPECT_EQ(completed_tasks.load(std::memory_order_relaxed), TASK_COUNT);
}

TEST_F(TaskDispatcherShutdownTest, ImmediateShutdownWithLongRunningTasks) {
    std::atomic<int> completed_tasks{0};
    bool long_task_started = false;
    bool long_task_finished = false;
    std::mutex long_task_mutex;
    std::condition_variable long_task_cv;

    dispatcher->Schedule(TaskPriority::High, [&]() {
        {
            std::lock_guard<std::mutex> lock(long_task_mutex);
            long_task_started = true;
        }
        std::this_thread::sleep_for(5s);
        {
            std::lock_guard<std::mutex> lock(long_task_mutex);
            long_task_finished = true;
            long_task_cv.notify_all();
        }
    });

    for (int i = 0; i < 3; ++i) {
        dispatcher->Schedule(TaskPriority::Normal, [&completed_tasks]() {
            std::this_thread::sleep_for(100ms);
            completed_tasks.fetch_add(1, std::memory_order_relaxed);
        });
    }

    dispatcher.reset();

    {
        std::unique_lock<std::mutex> lock(long_task_mutex);
        if (!long_task_cv.wait_for(lock, 6s, [&long_task_finished] { return long_task_finished; })) {
            FAIL() << "Долгая задача не завершилась за 6 секунд";
        }
    }

    EXPECT_GE(completed_tasks.load(std::memory_order_relaxed), 3);
    EXPECT_TRUE(long_task_started);
    EXPECT_TRUE(long_task_finished);
}
}  // namespace dispatcher