#pragma once
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

class VulkanRenderer;

/**
 * @namespace EntityCloner
 * @brief Utilities for cloning/duplicating ECS entities and their associated components.
 */
namespace EntityCloner {
    /**
     * @brief Creates a duplicate of an existing entity, copying its components.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     * @param source The source entity to clone.
     * @return The newly spawned clone entity.
     */
    Entity clone(Registry& registry, VulkanRenderer& renderer, Entity source);
}
