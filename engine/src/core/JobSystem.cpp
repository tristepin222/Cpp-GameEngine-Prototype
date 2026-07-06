#include "core/JobSystem.hpp"
#include <algorithm>
#include <iostream>

namespace Engine {

    void JobSystem::initialize(unsigned int threadCount) {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (initialized) return;

        if (threadCount == 0) {
            unsigned int hw = std::thread::hardware_concurrency();
            // Use hardware threads minus 1 (to leave main thread free), or at least 1
            threadCount = hw > 1 ? hw - 1 : 1;
        }

        stop = false;
        workers.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            workers.emplace_back(&JobSystem::workerThreadLoop, this);
        }

        initialized = true;
        std::cout << "[JobSystem] Initialized with " << threadCount << " worker threads." << std::endl;
    }

    void JobSystem::shutdown() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (!initialized) return;
            stop = true;
        }
        cv.notify_all();

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();

        // Clear remaining jobs
        std::queue<std::function<void()>> emptyQueue;
        std::swap(jobQueue, emptyQueue);

        initialized = false;
        std::cout << "[JobSystem] Shut down successfully." << std::endl;
    }

    JobSystem::~JobSystem() {
        shutdown();
    }

    void JobSystem::pushJob(std::function<void()> job) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            if (stop) return;
            jobQueue.push(std::move(job));
        }
        cv.notify_one();
    }

    void JobSystem::parallelFor(int count, std::function<void(int)> func) {
        if (count <= 0) return;
        
        unsigned int numThreads = 0;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            numThreads = workers.size();
        }

        if (numThreads == 0 || count < 2) {
            for (int i = 0; i < count; ++i) {
                func(i);
            }
            return;
        }

        unsigned int numChunks = std::min(numThreads, static_cast<unsigned int>(count));
        std::atomic<int> chunksRemaining(numChunks);

        int chunkSize = (count + numChunks - 1) / numChunks;

        for (unsigned int c = 0; c < numChunks; ++c) {
            int start = c * chunkSize;
            int end = std::min(start + chunkSize, count);

            pushJob([&func, start, end, &chunksRemaining]() {
                for (int i = start; i < end; ++i) {
                    func(i);
                }
                --chunksRemaining;
            });
        }

        // Spin-wait and yield thread slices until all background chunks have completed execution
        while (chunksRemaining.load(std::memory_order_relaxed) > 0) {
            std::this_thread::yield();
        }
    }

    void JobSystem::workerThreadLoop() {
        while (true) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this]() { return stop || !jobQueue.empty(); });
                if (stop && jobQueue.empty()) return;
                job = std::move(jobQueue.front());
                jobQueue.pop();
            }
            if (job) {
                try {
                    job();
                } catch (const std::exception& e) {
                    std::cerr << "[JobSystem] Worker thread caught exception: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "[JobSystem] Worker thread caught unknown exception." << std::endl;
                }
            }
        }
    }

} // namespace Engine
