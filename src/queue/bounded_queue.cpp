#include "queue/bounded_queue.hpp"
#include <mutex>

namespace dispatcher::queue {

BoundedQueue::BoundedQueue(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0)
        throw std::invalid_argument("Для BoundedQueue необходимо указать ёмкость больше 0");
}

void BoundedQueue::push(std::function<void()> task) {
    std::unique_lock<std::mutex> lock{mutex_};
    not_full_.wait(lock, [this] { return tasks_.size() < capacity_; });
    tasks_.emplace_back(std::move(task));
}

std::optional<std::function<void()>> BoundedQueue::try_pop() {
    std::unique_lock<std::mutex> lock{mutex_};
    if (tasks_.empty())
        return std::nullopt;
    auto task{tasks_.front()};
    tasks_.pop_front();
    not_full_.notify_one();
    return task;
}

std::size_t BoundedQueue::size() {
    std::unique_lock<std::mutex> lock{mutex_};
    return tasks_.size();
}

std::size_t BoundedQueue::capacity() {
    std::unique_lock<std::mutex> lock{mutex_};
    return capacity_;
}

}  // namespace dispatcher::queue