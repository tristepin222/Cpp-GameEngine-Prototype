#pragma once
#include <string>
#include <glm/glm.hpp>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/PrimitiveType.hpp"

class VulkanRenderer;

namespace EntityFactory {
    Entity spawnPrimitive(Registry& registry, VulkanRenderer& renderer, PrimitiveKind kind, const std::string& name, const glm::vec3& position);
    Entity spawnCamera(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, float fov);
    Entity spawnGrid(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, const glm::vec4& color, float spacing, float size);
    
    void uploadMesh(Registry& registry, VulkanRenderer& renderer, Entity entity);
}
