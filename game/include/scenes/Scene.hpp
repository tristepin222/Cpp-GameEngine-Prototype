#pragma once

#include <vector>

#include "../ecs/Entity.hpp"
#include "../ecs/Registry.hpp"

class VulkanRenderer;

class Scene {
public:
    Scene(Registry& registry, VulkanRenderer& renderer)
        : registry(registry), renderer(renderer) {
    }

    virtual ~Scene() = default;

    virtual void load() = 0;
    virtual void unload() {
        for (Entity entity : ownedEntities) {
            registry.destroy(entity);
        }
        ownedEntities.clear();
    }
    virtual void update(float dt) = 0;

protected:
    Entity trackEntity(Entity entity) {
        ownedEntities.push_back(entity);
        return entity;
    }

    Registry& registry;
    VulkanRenderer& renderer;

private:
    std::vector<Entity> ownedEntities;
};
