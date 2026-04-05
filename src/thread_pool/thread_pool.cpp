#include "thread_pool/thread_pool.hpp"

#include <optional>
#include <thread>

namespace dispatcher::thread_pool {

ThreadPool::~ThreadPool() {
    priority_queue_->shutdown();
    for (auto &thread : threads_) {
        auto ss{thread.get_stop_source()};
        ss.request_stop();
        thread.join();
    }
}

ThreadPool::ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> queue, std::size_t num_threads)
    : priority_queue_(std::move(queue)) {
    threads_.reserve(num_threads);
    for (std::size_t i = 0; i < num_threads; ++i)
        threads_.emplace_back([this](std::stop_token stoken) { Worker(stoken); });
}

void ThreadPool::Worker(std::stop_token stoken) {
    while (true) {
        auto task{priority_queue_->pop()};
        if (!task.has_value())
            break;

        task.value()();
    }
}

std::size_t ThreadPool::size() const { return threads_.size(); }

}  // namespace dispatcher::thread_pool
