#pragma once
#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <memory>
#include <semaphore>
#include <mutex>
#include <vector>


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

        // Tries to execute a single job from the queue (returns true if a job was executed)
        bool tryExecuteOneJob();

    private:
        JobSystem() = default;
        ~JobSystem();

        // Worker thread function loop
        void workerThreadLoop();

        std::vector<std::thread> workers;

        // Bounded MPMC queue cell structure
        struct Cell {
            std::atomic<size_t> sequence;
            std::function<void()> job;
        };

        std::unique_ptr<Cell[]> buffer;
        size_t bufferMask{0};

        // Align queue cursors on separate cache lines to avoid false sharing
        alignas(64) std::atomic<size_t> enqueuePos{0};
        alignas(64) std::atomic<size_t> dequeuePos{0};

        // Semaphore to wake worker threads without a mutex
        std::counting_semaphore<65536> jobSemaphore{0};

        std::atomic<bool> stop{false};
        std::atomic<bool> initialized{false};
    };

} // namespace Engine
