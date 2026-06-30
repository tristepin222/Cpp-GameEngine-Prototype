#pragma once
#include "System.hpp"
#include <memory>
#include <vector>

class SystemManager {
public:
    // Add a system (MovementSystem, RenderSystem, etc.)
    void addSystem(std::shared_ptr<System> system) {
        systems.push_back(system);
    }

    // Update all systems
    void updateAll(float dt) {
        for (auto& system : systems) {
            system->update(dt);
        }
    }

private:
    std::vector<std::shared_ptr<System>> systems;
};
