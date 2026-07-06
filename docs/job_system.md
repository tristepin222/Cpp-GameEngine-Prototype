# Multi-Threaded Job System

This document outlines the architecture and execution logic of the engine's Multi-Threaded Job System. The job system provides a lightweight thread pool scheduler for distributing loop iterations and independent work units across multi-core processors.

---

## Architecture Overview

The system is encapsulated by **[JobSystem.hpp](../engine/include/core/JobSystem.hpp)** and implemented in **[JobSystem.cpp](../engine/src/core/JobSystem.cpp)** as a thread-safe singleton. It maintains:
1. **Worker Threads**: A vector of active system threads (`std::vector<std::thread>`).
2. **Job Queue**: A FIFO queue (`std::queue<std::function<void()>>`) containing pending task closures.
3. **Queue Sync**: A mutex (`std::mutex`) guarding access to the queue, and a condition variable (`std::condition_variable`) to sleep idle workers.

---

## Execution Interface

### 1. Initialization & Lifecycle
On engine startup, `JobSystem::initialize(threadCount)` is called:
* If `threadCount` is `0`, it queries system hardware concurrency (`std::thread::hardware_concurrency()`) and spawns $N-1$ worker threads, leaving one core completely free for main thread loop operations.
* Workers run the internal `workerThreadLoop` where they sleep until tasks are pushed to the queue.
* On shutdown, `JobSystem::shutdown()` sets a `stop` flag, wakes up all sleeping workers, waits for them to join, and clears any remaining tasks.

### 2. Job Submission
Single tasks are pushed using `pushJob(std::function<void()> job)`:
```cpp
void JobSystem::pushJob(std::function<void()> job) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (stop) return;
        jobQueue.push(std::move(job));
    }
    cv.notify_one();
}
```

---

## Parallel For Loop Partitioning

The primary interface for parallel loop execution is:
```cpp
void parallelFor(int count, std::function<void(int)> func);
```
`parallelFor` splits a flat index loop into independent chunks executed concurrently across available threads.

### 1. Work Chunking
* If the job system contains no threads, or the loop count is $< 2$, it executes sequentially on the calling thread to avoid scheduling overhead.
* Otherwise, the loop is partitioned into $M$ chunks (where $M = \min(\text{threads}, \text{count})$).
* The chunk size is calculated dynamically:
  
  $$\text{chunk size} = \frac{\text{count} + M - 1}{M}$$

* Each chunk is pushed to the job queue as a lambda capture:
  ```cpp
  pushJob([&func, start, end, &chunksRemaining]() {
      for (int i = start; i < end; ++i) {
          func(i);
      }
      --chunksRemaining;
  });
  ```

---

## Cooperative Spin-Yield Synchronization

### The Condition Variable Race Condition (Postmortem)
In earlier versions, `parallelFor` used a stack-allocated mutex and condition variable on the calling thread to wait for background tasks:
```cpp
// DEPRECATED RACY APPROACH:
std::mutex waitMutex;
std::condition_variable waitCv;
// Workers notified waitCv when done...
```
* **The Bug**: If the calling thread woke up early or finished waiting and exited the function scope, the stack frame holding the condition variable and mutex was deallocated. Any late worker thread attempting to lock the destroyed mutex or signal the destroyed condition variable triggered memory access violations or OS-level `resource deadlock would occur` exceptions.

### The Lock-Free Yield-Spin Solution
To resolve this use-after-free race condition, the synchronization was refactored to use a lock-free atomic counter and a cooperative yield loop:
```cpp
std::atomic<int> chunksRemaining(numChunks);
// ... enqueue jobs ...

// Spin-wait and yield thread slices until all chunks finish
while (chunksRemaining.load(std::memory_order_relaxed) > 0) {
    std::this_thread::yield();
}
```
1. **No Stack-Allocated Sync Primitives**: The only shared synchronization state is the thread-safe `std::atomic<int> chunksRemaining` counter.
2. **Cooperative Yielding**: Instead of hard-spinning (which consumes 100% of a CPU core), the main thread calls `std::this_thread::yield()`. This cooperatively relinquishes the calling thread's current OS time slice, giving immediate execution priority to worker threads on the same core.
3. **Zero Deadlocks**: Once the atomic counter reaches zero, the function returns safely, eliminating all stack-corruption bugs.
