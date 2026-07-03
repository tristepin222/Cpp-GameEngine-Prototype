#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"

class VulkanRenderer;

/**
 * @struct Texture
 * @brief Holds Vulkan handles and metadata for a loaded texture resource.
 */
struct Texture {
    /** @brief Raw Vulkan image object. */
    VkImage image = VK_NULL_HANDLE;
    /** @brief Allocated device memory backing the image. */
    VkDeviceMemory memory = VK_NULL_HANDLE;
    /** @brief View to read image data in shaders. */
    VkImageView imageView = VK_NULL_HANDLE;
    /** @brief Sampler specifying filtering and wrapping modes. */
    VkSampler sampler = VK_NULL_HANDLE;
    /** @brief Descriptor set updated for shader binding. */
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    /** @brief Width in pixels. */
    int width = 0;
    /** @brief Height in pixels. */
    int height = 0;
    /** @brief Filepath from which texture was loaded. */
    std::string path;
};

/**
 * @class ResourceManager
 * @brief Manages loading, caching, and GPU memory uploads for textures and glTF model meshes.
 */
class ResourceManager {
public:
    /**
     * @brief Construct a new Resource Manager object.
     */
    ResourceManager() = default;
    /**
     * @brief Destroy the Resource Manager object.
     */
    ~ResourceManager() = default;

    /**
     * @brief Loads a texture from file, uploading it to the GPU and caching the resource.
     * @param path The texture file path.
     * @param renderer Reference to active VulkanRenderer.
     * @return Pointer to the loaded Texture object, or nullptr if failed.
     */
    Texture* loadTexture(const std::string& path, VulkanRenderer& renderer);

    /**
     * @brief Loads a glTF mesh from file, uploading geometry buffers to the GPU and caching it.
     * @param path The glTF/glb file path.
     * @param renderer Reference to active VulkanRenderer.
     * @return The loaded Mesh component.
     */
    Mesh loadMesh(const std::string& path, VulkanRenderer& renderer, int primitiveIndex = -1);

    /**
     * @brief Scans a glTF file and returns the count of primitive submesh parts.
     * @param path The glTF file path.
     * @return Number of primitive submesh parts.
     */
    int getMeshPrimitiveCount(const std::string& path);

    /**
     * @brief Loads skeletal skin and animations from a glTF file.
     * @param path The glTF/glb file path.
     * @param skeleton Target SkeletonComponent to populate.
     * @param animator Target AnimatorComponent to populate.
     * @return True if skin/animations were loaded successfully.
     */
    bool loadSkeletonAndAnimations(const std::string& path, SkeletonComponent& skeleton, AnimatorComponent& animator);

    /**
     * @brief Initializes the default 1x1 white texture for untextured material fallbacks.
     * @param renderer Reference to active VulkanRenderer.
     */
    void createDefaultWhiteTexture(VulkanRenderer& renderer);

    /**
     * @brief Retrieves the default fallback white texture pointer.
     * @return Pointer to default Texture.
     */
    Texture* getDefaultWhiteTexture() { return &defaultWhiteTexture; }

    /**
     * @brief Safely frees all cached textures and meshes from GPU memory.
     * @param device Vulkan logical device context.
     */
    void cleanup(VkDevice device);

    /**
     * @brief Finds a memory type matching properties in the physical GPU.
     */
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    /** @brief Cache of loaded textures mapped by file path. */
    std::unordered_map<std::string, std::unique_ptr<Texture>> textureCache;
    /** @brief Cache of loaded meshes mapped by file path. */
    std::unordered_map<std::string, Mesh> meshCache;
    /** @brief Fallback default 1x1 texture. */
    Texture defaultWhiteTexture;

private:
    /**
     * @brief Helper to load pixel data, create a VkImage, and copy staging data onto the GPU.
     */
    void createTextureImage(const std::string& path, VulkanRenderer& renderer, Texture& texture);
    /**
     * @brief Helper to create standard 2D image views.
     */
    void createTextureImageView(VkDevice device, Texture& texture);
    /**
     * @brief Helper to configure linear texture samplers.
     */
    void createTextureSampler(VkDevice device, Texture& texture);
};
