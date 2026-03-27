#include "task_dispatcher.hpp"

namespace dispatcher {

TaskDispatcher::TaskDispatcher(std::size_t thread_count,
                               const std::map<TaskPriority, dispatcher::queue::QueueOptions> &priority_to_options)
    : priority_queue_(std::make_shared<dispatcher::queue::PriorityQueue>(priority_to_options)),
      thread_pool_(std::make_unique<dispatcher::thread_pool::ThreadPool>(priority_queue_, thread_count)) {}

void TaskDispatcher::schedule(TaskPriority priority, std::function<void()> task) {
    priority_queue_->push(priority, std::move(task));
}

TaskDispatcher::~TaskDispatcher() {
    if (priority_queue_)
        priority_queue_->shutdown();
}

}  // namespace dispatcher
