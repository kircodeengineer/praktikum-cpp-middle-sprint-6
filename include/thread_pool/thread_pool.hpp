#pragma once

#include <memory>
#include <thread>
#include <vector>

#include "queue/priority_queue.hpp"

namespace dispatcher::thread_pool {

class ThreadPool {
private:
    std::shared_ptr<dispatcher::queue::PriorityQueue> priority_queue_;
    std::vector<std::jthread> threads_;

    void Worker(std::stop_token stoken);

public:
    explicit ThreadPool(std::shared_ptr<dispatcher::queue::PriorityQueue> queue, std::size_t num_threads);
    ~ThreadPool();

    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    std::size_t size() const;
};

}  // namespace dispatcher::thread_pool
