# TaskFlow-ThreadPool
A simple and robust thread pool implementation compatible with C++11, solving common pain points of basic thread pools and adapting to old compilers (e.g., old versions of Visual Studio/MinGW).

### Core Features

- ✅ C++11 Compatible: Uses C++11 standard features (std::thread, std::future, std::packaged_task, etc.), compatible with old compilers that do not support C++17 and above.

- ✅ Task Return Value Support: Encapsulates tasks through std::packaged_task and std::future, allowing callers to obtain task execution results or capture exceptions via future.get().

- ✅ Task Queue Capacity Limit: Prevents memory overflow by setting the maximum capacity of the task queue; when the queue is full, enqueue() will block and wait until there is free space.

- ✅ Exception Safety: Uses try-catch to capture all exceptions during task execution, avoiding worker thread termination; exception information is passed to the caller through std::future.

- ✅ Thread Safety Guarantee: Uses std::lock_guard (simplified initialization) to manage locks, avoiding deadlocks and ensuring thread-safe task enqueue and execution.

- ✅ Status Query Interface: Provides interfaces such as getThreadNum(), getTaskQueueSize(), isStopped() to query the thread pool status.

- ✅ Copy/Move Disabled: Disables copy and move constructors/assignment operators to avoid complex thread management issues.

### Solved Problems

- Resolved the "no matching constructor for std::lock_guard" error in old C++11 compilers.

- Replaced std::invoke_result_t (C++17) with std::result_of (C++11) for better compatibility.

- Fixed issues such as unrecognized class members (stop, max_task_queue_size, threads) caused by incomplete code or compiler compatibility.

- Avoided output garbled characters by using a global mutex to protect console output.

### Compilation & Usage

- Compilation Command (g++/MinGW): g++ -std=c++11 thread_pool.cpp -o thread_pool -lpthread

- Visual Studio: Set the C++ language standard to "ISO C++11 Standard (/std:c++11)" and compile directly.

- Usage: Create a ThreadPool instance, submit tasks via enqueue(), and obtain results using std::future.

### Example Scenarios

- Batch execution of concurrent tasks (e.g., file processing, network requests).

- Tasks that require obtaining execution results (e.g., calculation tasks).

- Projects that need to run on old C++11 compiler environments.

### Extensible Directions

- Task priority (modify task queue to std::priority_queue).

- Dynamic thread count adjustment (add setThreadNum() interface).

- Idle thread timeout exit (reduce resource occupation).

- Integration with logging libraries (e.g., spdlog).

This thread pool is an entry-level industrial implementation, simple, reliable, and easy to use, suitable for small and medium-sized C++11 projects that require concurrent task processing.
