#pragma once
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

namespace Engine {

    /**
     * @class JobSystem
     * @brief A lightweight thread pool scheduler for processing concurrent jobs in parallel.
     */
    class JobSystem {
    public:
        /**
         * @brief Get the singleton instance.
         */
        static JobSystem& getInstance() {
            static JobSystem instance;
            return instance;
        }

        /**
         * @brief Spawns worker threads.
         * @param threadCount Number of worker threads. Defaults to std::thread::hardware_concurrency() - 1.
         */
        void initialize(unsigned int threadCount = 0);

        /**
         * @brief Shuts down all worker threads and waits for them to join.
         */
        void shutdown();

        /**
         * @brief Enqueues a single void-returning task.
         */
        void pushJob(std::function<void()> job);

        /**
         * @brief Splits a total loop count into chunks and evaluates them in parallel.
         */
        void parallelFor(int count, std::function<void(int)> func);

        /**
         * @brief Returns active thread count.
         */
        unsigned int getThreadCount() const { return static_cast<unsigned int>(workers.size()); }

    private:
        JobSystem() = default;
        ~JobSystem();

        // Worker thread function loop
        void workerThreadLoop();

        std::vector<std::thread> workers;
        std::queue<std::function<void()>> jobQueue;
        std::mutex queueMutex;
        std::condition_variable cv;
        std::atomic<bool> stop{false};
        bool initialized{false};
    };

} // namespace Engine
