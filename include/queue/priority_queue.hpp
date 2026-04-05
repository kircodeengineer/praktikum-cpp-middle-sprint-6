#pragma once
#include "queue/bounded_queue.hpp"
#include "queue/unbounded_queue.hpp"
#include "types.hpp"

#include <atomic>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>

namespace dispatcher::queue {

class PriorityQueue {
private:
    std::unordered_map<TaskPriority, std::unique_ptr<IQueue>> queues_;
    std::vector<TaskPriority> priority_order_{TaskPriority::High, TaskPriority::Normal};
    std::mutex mutex_;
    std::condition_variable cond_var_;
    bool shutdown_{};
    bool is_empty_{true};

public:
    explicit PriorityQueue(const std::map<TaskPriority, QueueOptions> &priority_to_options);

    void push(TaskPriority priority, std::function<void()> task);
    std::optional<std::function<void()>> pop();
    void shutdown();
    bool empty();

    ~PriorityQueue();
};

}  // namespace dispatcher::queue