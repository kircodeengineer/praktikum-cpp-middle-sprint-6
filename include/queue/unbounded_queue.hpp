#pragma once
#include "queue/queue.hpp"
#include <functional>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class UnboundedQueue : public IQueue {
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    bool shutdown_{};
    QueueOptions options_;

public:
    explicit UnboundedQueue();

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~UnboundedQueue() override;

    std::size_t size();
};

}  // namespace dispatcher::queue