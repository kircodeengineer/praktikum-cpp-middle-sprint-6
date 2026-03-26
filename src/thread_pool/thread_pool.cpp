#include "thread_pool/thread_pool.hpp"

#include <optional>

namespace dispatcher::thread_pool {

ThreadPool::~ThreadPool() { priority_queue_->shutdown(); }

ThreadPool::ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> queue, std::size_t num_threads)
    : priority_queue_(std::move(queue)) {
    threads_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i)
        threads_.emplace_back(&ThreadPool::worker_routine, this, std::stop_token{});
}

void ThreadPool::worker_routine(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        auto task{priority_queue_->pop()};

        if (!task.has_value())
            break;

        task.value()();
    }
}

std::size_t ThreadPool::size() const { return threads_.size(); }

}  // namespace dispatcher::thread_pool
