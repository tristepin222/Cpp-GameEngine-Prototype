#pragma once
#include <string>
#include <vector>
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

class VulkanRenderer;

/**
 * @class SceneSerializer
 * @brief Serializes and deserializes ECS entities and their registered components to/from JSON files.
 */
class SceneSerializer {
public:
    /**
     * @brief Construct a new Scene Serializer object.
     * @param registry Reference to the ECS Registry.
     * @param renderer Reference to the Vulkan Renderer.
     */
    SceneSerializer(Registry& registry, VulkanRenderer& renderer);
    
    /**
     * @brief Serializes a list of entities and their components to a JSON file.
     * @param path Target filepath.
     * @param entities List of entities to save.
     * @return True if saving was successful, false otherwise.
     */
    bool serialize(const std::string& path, const std::vector<Entity>& entities);
    /**
     * @brief Deserializes entities and components from a JSON file.
     * @param path Source filepath.
     * @param outEntities Target list to fill with loaded entities.
     * @return True if loading was successful, false otherwise.
     */
    bool deserialize(const std::string& path, std::vector<Entity>& outEntities);

private:
    /** @brief Reference to registry. */
    Registry& registry;
    /** @brief Reference to Vulkan renderer. */
    VulkanRenderer& renderer;
};
