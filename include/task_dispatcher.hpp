#pragma once

#include <map>
#include <memory>

#include "queue/priority_queue.hpp"
#include "thread_pool/thread_pool.hpp"
#include "types.hpp"

namespace dispatcher {

namespace defaults {
inline constexpr std::size_t HIGH_PRIORITY_QUEUE_CAPACITY = 1000;
inline constexpr bool HIGH_PRIORITY_QUEUE_BOUNDED = true;
inline constexpr bool NORMAL_PRIORITY_QUEUE_BOUNDED = false;
}  // namespace defaults

class TaskDispatcher {
private:
    std::shared_ptr<dispatcher::queue::PriorityQueue> priority_queue_;
    std::unique_ptr<dispatcher::thread_pool::ThreadPool> thread_pool_;

public:
    explicit TaskDispatcher(std::size_t thread_count,
                            const std::map<TaskPriority, dispatcher::queue::QueueOptions> &priority_to_options = {
                                {TaskPriority::High,
                                 {defaults::HIGH_PRIORITY_QUEUE_BOUNDED, defaults::HIGH_PRIORITY_QUEUE_CAPACITY}},
                                {TaskPriority::Normal, {defaults::NORMAL_PRIORITY_QUEUE_BOUNDED, std::nullopt}}});

    void schedule(TaskPriority priority, std::function<void()> task);
    ~TaskDispatcher();
};

}  // namespace dispatcher
