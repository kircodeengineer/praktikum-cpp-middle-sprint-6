#include "queue/unbounded_queue.hpp"

namespace dispatcher::queue {
UnboundedQueue::UnboundedQueue() : options_(false) {}

UnboundedQueue::~UnboundedQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
}

void UnboundedQueue::push(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (shutdown_)
        return;
    tasks_.push(std::move(task));
}

std::optional<std::function<void()>> UnboundedQueue::try_pop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (tasks_.empty() || shutdown_)
        return std::nullopt;
    auto task{std::move(tasks_.front())};
    tasks_.pop();
    return task;
}

std::size_t UnboundedQueue::size() {
    std::unique_lock<std::mutex> lock{mutex_};
    return tasks_.size();
}

bool UnboundedQueue::empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.empty();
}

}  // namespace dispatcher::queue