#include "queue/bounded_queue.hpp"
#include <mutex>

namespace dispatcher::queue {

BoundedQueue::BoundedQueue(std::size_t capacity) : options_(true, capacity) {
    if (!options_.capacity || *options_.capacity == 0) {
        throw std::invalid_argument("Для BoundedQueue необходимо указать ёмкость больше 0");
    }
}

BoundedQueue::~BoundedQueue() {
    std::lock_guard<std::mutex> lock(mutex_);
    shutdown_ = true;
}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock{mutex_};
    if (shutdown_)
        return;
    not_full_.wait(lock, [this] { return tasks_.size() < *options_.capacity || shutdown_; });
    if (shutdown_)
        return;
    tasks_.push(std::move(task));
}

std::optional<std::function<void()>> BoundedQueue::try_pop() {
    std::unique_lock<std::mutex> lock{mutex_};
    if (tasks_.empty() || shutdown_)
        return std::nullopt;
    auto task{tasks_.front()};
    tasks_.pop();
    not_full_.notify_one();
    return task;
}

std::size_t BoundedQueue::size() {
    std::unique_lock<std::mutex> lock{mutex_};
    return tasks_.size();
}

std::size_t BoundedQueue::capacity() {
    std::unique_lock<std::mutex> lock{mutex_};
    return *options_.capacity;
}

bool BoundedQueue::empty() {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.empty();
}

}  // namespace dispatcher::queue