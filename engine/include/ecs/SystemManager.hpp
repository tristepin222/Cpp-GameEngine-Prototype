#pragma once
#include "System.hpp"
#include <memory>
#include <vector>

/**
 * @class SystemManager
 * @brief Manages registration and sequential updating of active ECS systems.
 */
class SystemManager {
public:
    /**
     * @brief Registers a new system with the manager.
     * @param system Shared pointer to the system to add.
     */
    void addSystem(std::shared_ptr<System> system) {
        systems.push_back(system);
    }

    /**
     * @brief Triggers the update routine on all registered systems.
     * @param dt Delta time in seconds.
     */
    void updateAll(float dt) {
        for (auto& system : systems) {
            system->update(dt);
        }
    }

    /**
     * @brief Removes and releases all registered systems.
     * Call this before destroying Vulkan resources to ensure systems
     * (which may own GPU pipelines or descriptors) are shut down first.
     */
    void clear() {
        systems.clear();
    }

private:
    /** @brief Collection of registered systems. */
    std::vector<std::shared_ptr<System>> systems;
};
