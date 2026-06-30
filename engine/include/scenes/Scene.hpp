#pragma once

#include <string>
#include <vector>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

class VulkanRenderer;

class Scene {
public:
    Scene(Registry& registry, VulkanRenderer& renderer);
    virtual ~Scene();

    virtual void load() = 0;
    virtual void update(float dt) = 0;
    virtual void unload();

    // Builtin default implementations of editor-called functions
    virtual bool saveToFile(const std::string& path);
    virtual bool loadFromFile(const std::string& path);
    virtual Entity createPrimitiveEntity(const std::string& primitiveType);
    virtual Entity createEntityOfType(const std::string& entityType);
    virtual Entity duplicateEntity(Entity entity);
    virtual bool deleteEntity(Entity entity);

protected:
    Entity trackEntity(Entity entity);
    void untrackEntity(Entity entity);
    
    Entity findEntityByName(const std::string& name) const;
    std::string makeUniqueEntityName(const std::string& baseName) const;

    Registry& registry;
    VulkanRenderer& renderer;

private:
    std::vector<Entity> ownedEntities;
};
