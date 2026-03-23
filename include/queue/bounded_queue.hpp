#pragma once
#include "queue/queue.hpp"
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <mutex>

namespace dispatcher::queue {

class BoundedQueue : public IQueue {
    std::deque<std::function<void()>> tasks_;
    const std::size_t capacity_;
    std::mutex mutex_;
    std::condition_variable not_full_;

public:
    explicit BoundedQueue(std::size_t capacity);

    void push(std::function<void()> task) override;

    std::optional<std::function<void()>> try_pop() override;

    ~BoundedQueue() override = default;

    std::size_t size();
    std::size_t capacity();
};

}  // namespace dispatcher::queue