#include "thread_pool.h"

/*
*  TODO: add <numThreads> worker threads to the workers vector, which will each do the following:
*  - remove the first task from the tasks queue and perform it
*  - allow other threads to use the task queue when it is not being modified
*  - correctly update activeTasks
*  - return once ALL tasks are completed AND stop is true
*  HINT: it is important to modify shared resources synchronously. queueMutex and condition are available to you
*/
ThreadPool::ThreadPool(size_t numThreads) : stop(false), activeTasks(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                std::unique_lock<std::mutex> lock(queueMutex);

                condition.wait(lock, [this] {
                    return stop || !tasks.empty();
                });

                if (stop && tasks.empty()) {
                    lock.unlock();  
                    return;
                }

                task = tasks.front();
                tasks.pop();
                activeTasks++;

                lock.unlock();

                task();
                activeTasks--;
            }
        });
    }
}


/*
*  TODO: once there are no tasks to be done, set stop to true to stop adding tasks
*  notify all threads once this is done so that they can be deleted
*  join worker threads once all tasks are complete
*/
ThreadPool::~ThreadPool() {

    std::unique_lock<std::mutex> lock(queueMutex);
    stop = true;
    lock.unlock();
        
    condition.notify_all();
    for (std::thread &worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

/*
*  TODO: add a task to the tasks queue, and notify a thread that a task is available
*/
void ThreadPool::enqueue(std::function<void()> task) {
    std::unique_lock<std::mutex> lock(queueMutex);
    if (stop) {
        lock.unlock();
        return;
    }
    tasks.emplace(task);
    lock.unlock();
    condition.notify_one();
    
}