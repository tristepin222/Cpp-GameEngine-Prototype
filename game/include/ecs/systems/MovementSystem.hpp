// include/ecs/systems/MovementSystem.hpp
#pragma once
#include "../System.hpp"
#include "../Registry.hpp"
#include "../components/Transform.hpp"
#include "../components/Velocity.hpp"

class MovementSystem : public System {
public:
    MovementSystem(Registry& reg) : registry(reg) {
        // subscribe to entities that gain a Transform + Velocity
        registry.subscribeToAdded<Transform>([this](Entity e) { checkAndAdd(e); });
        registry.subscribeToAdded<Velocity>([this](Entity e) { checkAndAdd(e); });

        registry.subscribeToRemoved<Transform>([this](Entity e) { removeEntity(e); });
        registry.subscribeToRemoved<Velocity>([this](Entity e) { removeEntity(e); });
    }

    void update(float dt) override {
        for (Entity e : entities) {
            auto* t = registry.get<Transform>(e);
            auto* v = registry.get<Velocity>(e);
            if (t && v) {
                t->position += v->value * dt;
            }
        }
    }

private:
    Registry& registry;

    void checkAndAdd(Entity e) {
        auto* t = registry.get<Transform>(e);
        auto* v = registry.get<Velocity>(e);
        if (t && v) {
            // only add if both exist
            if (std::find(entities.begin(), entities.end(), e) == entities.end())
                entities.push_back(e);
        }
    }
};
