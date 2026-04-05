#pragma once
#include "queue/queue.hpp"
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <queue>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
    std::queue<std::function<void()>> tasks_;
    QueueOptions options_;
    std::mutex mutex_;
    std::condition_variable not_full_;
    bool shutdown_{};

public:
    explicit BoundedQueue(std::size_t capacity);

    void push(std::function<void()> task) override;

    [[nodiscard]] std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override;

    std::size_t size() override;
    std::size_t capacity();
    bool empty() override;
};

}  // namespace dispatcher::queue