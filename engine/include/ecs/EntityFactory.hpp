#pragma once
#include <string>
#include <glm/glm.hpp>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/components/PrimitiveType.hpp"

class VulkanRenderer;

/**
 * @namespace EntityFactory
 * @brief Factory functions to create, configure, and initialize typical entities and their Vulkan rendering resources.
 */
namespace EntityFactory {
    /**
     * @brief Spawns an entity with a basic geometric primitive mesh.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param kind The kind of primitive shape (Triangle, Cube, Quad).
     * @param name Name of the spawned entity.
     * @param position Initial position of the entity.
     * @return The spawned Entity object.
     */
    Entity spawnPrimitive(Registry& registry, VulkanRenderer& renderer, PrimitiveKind kind, const std::string& name, const glm::vec3& position);
    
    /**
     * @brief Spawns a camera entity.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param name Name of the camera entity.
     * @param position Initial position of the camera.
     * @param rotation Initial rotation of the camera.
     * @param fov Field of view in degrees.
     * @return The spawned Entity object.
     */
    Entity spawnCamera(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, float fov);
    
    /**
     * @brief Spawns a grid entity used for rendering editor grids.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param name Name of the grid entity.
     * @param position Initial position of the grid.
     * @param rotation Initial rotation of the grid.
     * @param color Base color of grid lines.
     * @param spacing Line spacing.
     * @param size Overall grid size.
     * @return The spawned Entity object.
     */
    Entity spawnGrid(Registry& registry, VulkanRenderer& renderer, const std::string& name, const glm::vec3& position, const glm::vec3& rotation, const glm::vec4& color, float spacing, float size);
    
    /**
     * @brief Allocates and uploads GPU buffers for a mesh component on the specified entity.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param entity Entity containing the Mesh component to upload.
     */
    void uploadMesh(Registry& registry, VulkanRenderer& renderer, Entity entity);
}
