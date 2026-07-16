#include "core/JobSystem.hpp"
#include <algorithm>
#include <iostream>
#include <atomic>
#include <semaphore>
#include <memory>
#include <limits>
#include <cstdint>
#include <thread>

#if defined(_MSC_VER)
#include <intrin.h>
#define SPIN_PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
#define SPIN_PAUSE() __builtin_ia32_pause()
#else
#define SPIN_PAUSE() do {} while(0)
#endif

namespace Engine {

    void JobSystem::initialize(unsigned int threadCount) {
        if (initialized.load(std::memory_order_acquire)) return;

        if (threadCount == 0) {
            unsigned int hw = std::thread::hardware_concurrency();
            // Use hardware threads minus 1 (to leave main thread free), or at least 1
            threadCount = hw > 1 ? hw - 1 : 1;
        }

        // Initialize Dmitriy Vyukov's MPMC bounded queue
        const size_t bufferSize = 65536; // Must be a power of two
        buffer = std::make_unique<Cell[]>(bufferSize);
        bufferMask = bufferSize - 1;
        for (size_t i = 0; i < bufferSize; ++i) {
            buffer[i].sequence.store(i, std::memory_order_relaxed);
        }
        enqueuePos.store(0, std::memory_order_relaxed);
        dequeuePos.store(0, std::memory_order_relaxed);

        stop.store(false, std::memory_order_release);
        workers.reserve(threadCount);
        for (unsigned int i = 0; i < threadCount; ++i) {
            workers.emplace_back(&JobSystem::workerThreadLoop, this);
        }

        initialized.store(true, std::memory_order_release);
        std::cout << "[JobSystem] Lock-free pre-allocated JobSystem initialized with " << threadCount << " worker threads." << std::endl;
    }

    void JobSystem::shutdown() {
        if (!initialized.load(std::memory_order_acquire)) return;

        // Signal shutdown and wake all workers
        stop.store(true, std::memory_order_release);
        jobSemaphore.release(static_cast<int>(workers.size()));

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers.clear();

        // Clear pre-allocated queue buffer jobs
        buffer.reset();
        bufferMask = 0;
        enqueuePos.store(0, std::memory_order_relaxed);
        dequeuePos.store(0, std::memory_order_relaxed);

        initialized.store(false, std::memory_order_release);
        std::cout << "[JobSystem] Lock-free JobSystem shut down successfully." << std::endl;
    }

    JobSystem::~JobSystem() {
        shutdown();
    }

    void JobSystem::pushJob(std::function<void()> job) {
        if (stop.load(std::memory_order_acquire)) return;

        size_t pos = enqueuePos.load(std::memory_order_relaxed);
        while (true) {
            Cell* cell = &buffer[pos & bufferMask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0) {
                if (enqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    cell->job = std::move(job);
                    cell->sequence.store(pos + 1, std::memory_order_release);
                    break;
                }
            } else if (diff < 0) {
                // Queue is full! Spin and pause before loading again.
                for (int spin = 0; spin < 64; ++spin) {
                    SPIN_PAUSE();
                }
                std::this_thread::yield();
                pos = enqueuePos.load(std::memory_order_relaxed);
            } else {
                pos = enqueuePos.load(std::memory_order_relaxed);
            }
        }

        // Wake a worker without locking
        jobSemaphore.release();
    }

    bool JobSystem::tryExecuteOneJob() {
        size_t pos = dequeuePos.load(std::memory_order_relaxed);
        Cell* cell = nullptr;
        while (true) {
            cell = &buffer[pos & bufferMask];
            size_t seq = cell->sequence.load(std::memory_order_acquire);
            intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
            if (diff == 0) {
                if (dequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    break;
                }
            } else if (diff < 0) {
                // Queue is empty!
                return false;
            } else {
                pos = dequeuePos.load(std::memory_order_relaxed);
            }
        }

        std::function<void()> job = std::move(cell->job);
        cell->sequence.store(pos + bufferMask + 1, std::memory_order_release);

        if (job) {
            try {
                job();
            } catch (const std::exception& e) {
                std::cerr << "[JobSystem] Job execution caught exception: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "[JobSystem] Job execution caught unknown exception." << std::endl;
            }
        }
        return true;
    }

    void JobSystem::parallelFor(int count, std::function<void(int)> func) {
        if (count <= 0) return;
        unsigned int numThreads = workers.size();

        if (numThreads == 0 || count < 2) {
            for (int i = 0; i < count; ++i) func(i);
            return;
        }

        std::atomic<int> nextIndex{0};
        std::atomic<int> workersRemaining(static_cast<int>(numThreads));

        for (unsigned int w = 0; w < numThreads; ++w) {
            pushJob([&func, count, &nextIndex, &workersRemaining]() {
                while (true) {
                    int i = nextIndex.fetch_add(1, std::memory_order_relaxed);
                    if (i >= count) break;
                    func(i);
                }
                --workersRemaining;
            });
        }

        // Wait loop: main thread helps execute queued jobs while waiting for workers to complete
        while (workersRemaining.load(std::memory_order_acquire) > 0) {
            if (tryExecuteOneJob()) {
                continue;
            }
            SPIN_PAUSE();
            std::this_thread::yield();
        }
    }

    void JobSystem::workerThreadLoop() {
        while (true) {
            // Wait until a job becomes available or shutdown is signaled
            jobSemaphore.acquire();

            if (stop.load(std::memory_order_acquire)) {
                // Pop and execute remaining jobs (if any) before exiting
                while (tryExecuteOneJob()) {}
                return;
            }

            tryExecuteOneJob();
        }
    }

} // namespace Engine
