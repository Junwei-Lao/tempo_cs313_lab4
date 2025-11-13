#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <set>
#include <chrono>
#include <functional>
#include <iomanip>
#include <cstdlib>
#include <unistd.h>
#include "thread_pool.h"
#include "signals.h"

// Helper function to print test results
void print_test_result(const std::string& test_name, bool success) {
    std::cout << "TEST: " << test_name << " - ";
    
    if (success) {
        std::cout << "PASSED ✓" << std::endl;
    } else {
        std::cout << "FAILED ✗" << std::endl;
    }
}

// Test 1: Verify ThreadPool Constructor creates working threads
void test_constructor() {
    std::cout << "\n======== Testing ThreadPool Constructor ========" << std::endl;
    
    const size_t num_threads = 4;
    std::atomic<int> counter(0);
    std::mutex mtx;
    std::condition_variable cv;
    int threads_completed = 0;
    bool all_tasks_executed = false;

    std::cout << "Creating ThreadPool with " << num_threads << " threads..." << std::endl;
    
    {
        // Create ThreadPool
        ThreadPool pool(num_threads);
        std::cout << "ThreadPool created successfully." << std::endl;
        
        // Enqueue tasks to verify threads are working
        std::cout << "Enqueueing " << num_threads << " tasks..." << std::endl;
        int time[] = {10, 3, 6, 2};
        for (size_t i = 0; i < num_threads; i++) {
            pool.enqueue([&counter, &mtx, &cv, &threads_completed, i, &time]() {
                std::cout << "  Thread executing task " << i << std::endl;
                
                // Increment counter
                counter++;
                // sleep(rand() % 10);
                sleep(time[i]);
                
                // Signal completion
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    threads_completed++;
                    std::cout << "  Thread finished task " << i << " (total: " << threads_completed << ")" << std::endl;
                }
                cv.notify_one();
            });
        }
        
        // Wait for all worker threads to complete
        {
            std::unique_lock<std::mutex> lock(mtx);
            std::cout << "Waiting for all tasks to complete..." << std::endl;
            
            if (cv.wait_for(lock, std::chrono::seconds(20), [&threads_completed, num_threads]() { 
                return threads_completed == num_threads;
            })) {
                std::cout << "All tasks completed within timeout." << std::endl;
                all_tasks_executed = true;
            } else {
                std::cout << "Timeout waiting for tasks to complete!" << std::endl;
            }
        }
        
        std::cout << "Final counter value: " << counter.load() << " (expected: " << num_threads << ")" << std::endl;
    }
    
    std::cout << "ThreadPool destroyed." << std::endl;
    
    // Evaluate test result
    bool test_passed = (counter.load() == num_threads) && all_tasks_executed;
    print_test_result("ThreadPool Constructor", test_passed);
}

// Test 2: Verify ThreadPool Destructor waits for tasks to complete
void test_destructor() {
    std::cout << "\n======== Testing ThreadPool Destructor ========" << std::endl;
    
    const size_t num_tasks = 5;
    std::atomic<int> counter(0);
    std::mutex mtx;
    std::condition_variable cv;
    bool start_tasks = false;
    
    // Vector to track task completion
    std::vector<bool> task_complete(num_tasks, false);
    
    std::cout << "Creating ThreadPool with 2 threads..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    {
        // Create ThreadPool
        ThreadPool pool(2);
        
        // Enqueue tasks that will wait for a signal
        std::cout << "Enqueueing " << num_tasks << " tasks that will wait for signal..." << std::endl;
        for (size_t i = 0; i < num_tasks; i++) {
            pool.enqueue([i, &counter, &mtx, &cv, &start_tasks, &task_complete]() {
                // Wait for signal to start
                {
                    std::unique_lock<std::mutex> lock(mtx);
                    std::cout << "  Task " << i << " waiting for signal..." << std::endl;
                    cv.wait(lock, [&start_tasks]() { return start_tasks; });
                }
                
                // Simulate work
                std::cout << "  Task " << i << " starting work..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // Mark as complete
                counter++;
                task_complete[i] = true;
                std::cout << "  Task " << i << " completed (total: " << counter.load() << ")" << std::endl;
            });
        }
        
        // Signal tasks to start in a separate thread
        std::thread signaler([&mtx, &cv, &start_tasks]() {
            // Wait a bit before signaling
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            {
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << "Signaling tasks to start..." << std::endl;
                start_tasks = true;
            }
            cv.notify_all();
        });
        
        // Detach signaler
        signaler.detach();
        
        std::cout << "About to destroy ThreadPool - should wait for tasks..." << std::endl;
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "ThreadPool destroyed after " << duration.count() << "ms" << std::endl;
    std::cout << "Final counter value: " << counter.load() << " (expected: " << num_tasks << ")" << std::endl;
    
    // Check all tasks completed
    bool all_complete = true;
    for (size_t i = 0; i < num_tasks; i++) {
        if (!task_complete[i]) {
            std::cout << "Task " << i << " did NOT complete!" << std::endl;
            all_complete = false;
        }
    }
    
    if (all_complete) {
        std::cout << "All tasks completed successfully." << std::endl;
    }
    
    // Evaluate test result
    bool test_passed = (counter.load() == num_tasks) && all_complete && (duration.count() >= 500);
    print_test_result("ThreadPool Destructor", test_passed);
}

// Test 3: Verify ThreadPool Enqueue can handle concurrent producers
void test_enqueue() {
    std::cout << "\n======== Testing ThreadPool Enqueue ========" << std::endl;
    
    const size_t num_producers = 4;
    const size_t tasks_per_producer = 100;
    const size_t total_tasks = num_producers * tasks_per_producer;
    
    // Thread-safe set implementation
    std::set<int> result_set;
    std::mutex set_mutex;
    
    auto add_to_set = [&](int value) {
        std::lock_guard<std::mutex> lock(set_mutex);
        result_set.insert(value);
    };
    
    std::atomic<int> completed_tasks(0);
    
    std::cout << "Creating ThreadPool with 2 threads..." << std::endl;
    ThreadPool pool(2);
    
    std::cout << "Creating " << num_producers << " producer threads..." << std::endl;
    std::vector<std::thread> producers;
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (size_t producer_id = 0; producer_id < num_producers; producer_id++) {
        producers.emplace_back([producer_id, tasks_per_producer, &pool, &add_to_set, &completed_tasks]() {
            std::cout << "Producer " << producer_id << " starting..." << std::endl;
            
            for (size_t i = 0; i < tasks_per_producer; i++) {
                int value = producer_id * tasks_per_producer + i;
                
                pool.enqueue([value, &add_to_set, &completed_tasks]() {
                    add_to_set(value);
                    completed_tasks++;
                });
            }
            
            std::cout << "Producer " << producer_id << " finished enqueueing tasks." << std::endl;
        });
    }
    
    // Wait for all producers to finish
    for (auto& thread : producers) {
        thread.join();
    }
    
    std::cout << "All producers finished. Waiting for tasks to complete..." << std::endl;
    
    // Poll until all tasks complete or timeout
    bool all_tasks_completed = false;
    auto timeout = std::chrono::seconds(10);
    auto poll_interval = std::chrono::milliseconds(100);
    auto end_time = start_time + timeout;
    
    while (std::chrono::steady_clock::now() < end_time) {
        if (completed_tasks.load() == total_tasks) {
            all_tasks_completed = true;
            break;
        }
        std::cout << "  Completed: " << completed_tasks.load() << "/" << total_tasks << " tasks" << std::endl;
        std::this_thread::sleep_for(poll_interval);
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    
    std::cout << "Tasks completed: " << completed_tasks.load() << "/" << total_tasks
              << " in " << duration.count() << "ms" << std::endl;
    
    // Check result set size
    size_t result_size;
    {
        std::lock_guard<std::mutex> lock(set_mutex);
        result_size = result_set.size();
    }
    
    std::cout << "Result set size: " << result_size << " (expected: " << total_tasks << ")" << std::endl;
    
    // Evaluate test result
    bool test_passed = all_tasks_completed && (result_size == total_tasks);
    print_test_result("ThreadPool Enqueue", test_passed);
}

// Main function to run all tests
int main() {
    SignalHandling::block_signals();
    std::cout << "===== ThreadPool Tests =====" << std::endl;
    
    test_constructor();
    test_destructor();
    test_enqueue();
    
    SignalHandling::unblock_signals();
    
    return 0;
}