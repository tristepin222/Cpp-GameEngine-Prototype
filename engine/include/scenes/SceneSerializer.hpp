#pragma once
#include <string>
#include <vector>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

class VulkanRenderer;

class SceneSerializer {
public:
    SceneSerializer(Registry& registry, VulkanRenderer& renderer);
    
    bool serialize(const std::string& path, const std::vector<Entity>& entities);
    bool deserialize(const std::string& path, std::vector<Entity>& outEntities);

private:
    Registry& registry;
    VulkanRenderer& renderer;
};
