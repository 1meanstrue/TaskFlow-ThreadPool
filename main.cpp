#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<vector>
#include<functional>
#include<future>
#include<chrono>
#include<memory>
#include<stdexcept>
#include<string>

// Mutex for protecting console output (avoid messy interleaved prints)
std::mutex globalCoutMtx;

// Thread pool class: manages worker threads to execute tasks asynchronously
class ThreadPool {
public:
    // Constructor: create thread pool with specified thread count and max task queue size
    ThreadPool(int numThreads, size_t maxTaskQueueSize)
        : stop(false), max_task_queue_size(maxTaskQueueSize) {
        // Validate input parameters
        if (numThreads <= 0) {
            throw std::invalid_argument("Thread number must be greater than 0.");
        }
        if (maxTaskQueueSize == 0) {
            throw std::invalid_argument("Max task queue size must be greater than 0.");
        }
        // Initialize worker threads
        createWorkerThreads(numThreads);
    }

    // Overloaded constructor: use default max task queue size (1000)
    explicit ThreadPool(int numThreads)
        : ThreadPool(numThreads, 1000) {}

    // Disable copy/move operations (thread pool can't be copied/moved)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // Destructor: stop thread pool and join all worker threads gracefully
    ~ThreadPool() {
        {
            // Lock to modify stop flag safely
            std::unique_lock<std::mutex> lock(mtx);
            stop = true;
        }
        // Wake up all waiting worker threads
        condition.notify_all();
        // Join all available threads to avoid resource leaks
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

    // Enqueue a task to the thread pool, return future to get task result
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type> {
        using ReturnType = typename std::result_of<F(Args...)>::type;

        // Package task and arguments into a shared packaged_task (extend lifetime to worker thread)
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        // Get future object for task result retrieval
        std::future<ReturnType> taskFuture = task->get_future();

        {
            std::unique_lock<std::mutex> lock(mtx);

            // Wait until queue has space or thread pool is stopped
            condition.wait(lock, [this]() {
                return stop || tasks.size() < max_task_queue_size;
                });

            // Reject task if thread pool is already stopped
            if (stop) {
                throw std::runtime_error("Cannot enqueue tasks to a stopped ThreadPool.");
            }

            // Add task to queue with exception handling (prevent worker thread crash)
            tasks.emplace([task]() {
                try {
                    (*task)(); // Execute the task
                }
                catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(globalCoutMtx);
                    std::cerr << "Task execution failed with standard exception: " << e.what() << std::endl;
                }
                catch (...) {
                    std::lock_guard<std::mutex> lock(globalCoutMtx);
                    std::cerr << "Task execution failed with unknown exception." << std::endl;
                }
                });
        }

        // Wake up one waiting worker thread to process the new task
        condition.notify_one();
        return taskFuture;
    }

    // Get current number of worker threads in the pool
    int getThreadNum() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx));
        return static_cast<int>(threads.size());
    }

    // Get current size of the pending task queue
    size_t getTaskQueueSize() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx));
        return tasks.size();
    }

    // Check if the thread pool has been stopped
    bool isStopped() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mtx));
        return stop;
    }

private:
    std::vector<std::thread> threads;    // Storage for worker threads
    std::queue<std::function<void()>> tasks; // Pending task queue
    std::mutex mtx;                      // Mutex for protecting shared resources
    std::condition_variable condition;   // Condition variable for thread synchronization
    bool stop;                           // Flag to indicate if thread pool is stopped
    size_t max_task_queue_size;          // Maximum capacity of the task queue

    // Create specified number of worker threads
    void createWorkerThreads(int numThreads) {
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(mtx);
                        // Wait until there are tasks to process or thread pool is stopped
                        condition.wait(lock, [this]() {
                            return !tasks.empty() || stop;
                            });

                        // Exit loop if pool is stopped and no pending tasks
                        if (stop && tasks.empty()) {
                            return;
                        }

                        // Get the first task from queue and remove it
                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    // Execute the task outside the lock (improve concurrency)
                    task();
                }
                });
        }
    }
};

// Helper function: convert integer to string
std::string intToStr(int num) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", num);
    return std::string(buf);
}

// Test task: print task info and sleep for a short time
void printTask(int taskId) {
    {
        std::lock_guard<std::mutex> lock(globalCoutMtx);
        std::cout << "Task " << taskId << " is running (thread ID: "
            << std::this_thread::get_id() << ")" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    {
        std::lock_guard<std::mutex> lock(globalCoutMtx);
        std::cout << "Task " << taskId << " is done (thread ID: "
            << std::this_thread::get_id() << ")" << std::endl;
    }
}

// Test task: calculate sum of two integers with a short delay
int addTask(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    return a + b;
}

// Test task: throw an intentional exception for error handling verification
void exceptionTask(int taskId) {
    throw std::runtime_error("Task " + intToStr(taskId) + " intentionally throws an exception.");
}

// Main function: test the thread pool functionality
int main() {
    try {
        ThreadPool pool(4, 20); // Create thread pool with 4 workers and 20 max task queue size

        // Print initial state of the thread pool
        std::cout << "=== ThreadPool Initial State ===" << std::endl;
        std::cout << "Current thread count: " << pool.getThreadNum() << std::endl;
        std::cout << "Current task queue size: " << pool.getTaskQueueSize() << std::endl;
        std::cout << "Is stopped: " << (pool.isStopped() ? "Yes" : "No") << std::endl;
        std::cout << "=================================" << std::endl << std::endl;

        // Enqueue print tasks
        std::vector<std::future<void>> voidFutures;
        for (int i = 0; i < 10; ++i) {
            voidFutures.emplace_back(pool.enqueue(printTask, i));
        }

        // Enqueue add tasks
        std::vector<std::future<int>> addFutures;
        for (int i = 0; i < 5; ++i) {
            addFutures.emplace_back(pool.enqueue(addTask, i, i * 2));
        }

        // Enqueue exception task
        auto exceptionFuture = pool.enqueue(exceptionTask, 99);

        // Wait for all print tasks to complete
        for (auto& fut : voidFutures) {
            fut.get();
        }

        // Print results of add tasks
        std::cout << std::endl << "=== Add Task Results ===" << std::endl;
        for (size_t i = 0; i < addFutures.size(); ++i) {
            int result = addFutures[i].get();
            std::cout << "Task " << i << " (a=" << i << ", b=" << i * 2 << ") result: " << result << std::endl;
        }

        // Catch exception from exception task
        try {
            exceptionFuture.get();
        }
        catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(globalCoutMtx);
            std::cerr << std::endl << "Caught exception from task 99: " << e.what() << std::endl;
        }

        // Print final state of the thread pool
        std::cout << std::endl << "=== ThreadPool Final State ===" << std::endl;
        std::cout << "Current thread count: " << pool.getThreadNum() << std::endl;
        std::cout << "Current task queue size: " << pool.getTaskQueueSize() << std::endl;
        std::cout << "Is stopped: " << (pool.isStopped() ? "Yes" : "No") << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Main function caught exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}