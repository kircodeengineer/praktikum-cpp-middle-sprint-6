#include "queue/priority_queue.hpp"
#include <cstddef>
#include <print>

namespace dispatcher::queue {

PriorityQueue::PriorityQueue(const std::map<TaskPriority, QueueOptions> &priority_to_options) {
    for (const auto &[priority, options] : priority_to_options) {
        if (options.bounded) {
            options.capacity
                .and_then([this, priority](auto value) {
                    queues_[priority] = std::make_unique<BoundedQueue>(value);
                    return std::optional<std::size_t>(value);
                })
                .or_else([]() {
                    throw std::invalid_argument("Нельзя добавить в базу ограниченную очередь без заданого объёма");
                    return std::optional<std::size_t>();
                });
        } else {
            options.capacity
                .and_then([this, priority](auto value) {
                    throw std::invalid_argument("Нельзя добавить в базу неограниченную очередь с заданным объёмом");
                    return std::optional<std::size_t>();
                })
                .or_else([this, priority]() {
                    queues_[priority] = std::make_unique<UnboundedQueue>();
                    return std::optional<std::size_t>();
                });
        }
    }
}

void PriorityQueue::push(TaskPriority priority, std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it{queues_.find(priority)};
        if (it == queues_.end())
            throw std::invalid_argument("Очереди с указанным приоритетом нет в базе");

        it->second->push(std::move(task));
        is_empty_ = false;
    }
    cond_var_.notify_one();
}

std::optional<std::function<void()>> PriorityQueue::pop() {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (shutdown_ && is_empty_)
                return std::nullopt;

            for (const auto &priority : priority_order_) {
                auto it{queues_.find(priority)};
                if (it != queues_.end()) {
                    auto task{it->second->try_pop()};
                    if (task.has_value())
                        return task;
                }
            }

            cond_var_.wait(lock, [this]() {
                for (const auto &priority : priority_order_) {
                    auto it{queues_.find(priority)};
                    if (it != queues_.end() && !it->second->empty())
                        return true;
                }
                is_empty_ = true;
                return shutdown_;
            });
        }
    }
}

void PriorityQueue::shutdown() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cond_var_.notify_all();
}

bool PriorityQueue::empty() {
    bool result{};
    {
        std::unique_lock<std::mutex> lock(mutex_);
        result = is_empty_;
    }
    return result;
}

PriorityQueue::~PriorityQueue() { shutdown(); }

}  // namespace dispatcher::queue
