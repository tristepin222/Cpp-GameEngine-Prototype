#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <unordered_map>
#include <memory>
#include "ecs/components/Mesh.hpp"
#include "ecs/components/Skeleton.hpp"
#include "ecs/components/Animator.hpp"
#include "ecs/components/Material.hpp"

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
    /** @brief Filter mode used for this texture. */
    TextureFilterMode filterMode = TextureFilterMode::Bilinear;
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
     * @param filterMode Desired filtering mode (will be overridden by .meta if exists).
     * @return Pointer to the loaded Texture object, or nullptr if failed.
     */
    Texture* loadTexture(const std::string& path, VulkanRenderer& renderer, TextureFilterMode filterMode = TextureFilterMode::Bilinear);

    /**
     * @brief Loads a glTF mesh from file, uploading geometry buffers to the GPU and caching it.
     * @param path The glTF/glb file path.
     * @param renderer Reference to active VulkanRenderer.
     * @return The loaded Mesh component.
     */
    Mesh loadMesh(const std::string& path, VulkanRenderer& renderer, int primitiveIndex = -1);

    /**
     * @brief Clears the cached mesh for a specific path to force a re-import.
     * @param path The file path.
     */
    void clearMeshCache(const std::string& path);

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
    bool loadSkeletonAndAnimations(const std::string& path, SkeletonComponent& skeleton, AnimatorComponent& animator, bool append = false);

    /**
     * @brief Loads skeletal skin and animations from a custom binary animation file (.anim).
     * @param path File path of the binary animation.
     * @param skeleton Target SkeletonComponent to populate.
     * @param animator Target AnimatorComponent to populate.
     * @param append If true, animation clips are appended instead of clearing the animator's cache.
     * @return True if loaded successfully.
     */
    bool loadBinarySkeletonAndAnimations(const std::string& path, SkeletonComponent& skeleton, AnimatorComponent& animator, bool append = false);

    /**
     * @brief Saves skeletal skin and animations to a custom binary animation file (.anim).
     * @param path File path to save the binary animation.
     * @param skeleton Source SkeletonComponent.
     * @param animator Source AnimatorComponent.
     * @return True if saved successfully.
     */
    bool saveBinarySkeletonAndAnimations(const std::string& path, const SkeletonComponent& skeleton, const AnimatorComponent& animator);

    /**
     * @brief Updates an already loaded texture's filter settings live and recreates its Vulkan sampler.
     * @param path The texture file path.
     * @param renderer Reference to VulkanRenderer.
     * @param filterMode The new texture filter mode to apply.
     */
    void updateTextureFilterMode(const std::string& path, VulkanRenderer& renderer, TextureFilterMode filterMode);

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
     * @brief Creates and caches a Vulkan texture directly from raw RGBA pixel data.
     *        Used by TilesetAsset::buildAtlas() to upload the packed tile atlas.
     * @param cacheKey  Unique string key used for the texture cache (e.g. "tileset:forest").
     * @param pixels    Pointer to RGBA8 pixel data (4 bytes per pixel, row-major).
     * @param width     Width in pixels.
     * @param height    Height in pixels.
     * @param renderer  Active VulkanRenderer.
     * @param filterMode Desired sampler filter mode.
     * @return Pointer to the uploaded Texture, or nullptr on failure.
     */
    Texture* createTextureFromPixels(const std::string& cacheKey,
                                     const uint8_t* pixels, int width, int height,
                                     VulkanRenderer& renderer,
                                     TextureFilterMode filterMode = TextureFilterMode::Nearest);

    /**
     * @brief Removes a texture from the cache and frees its GPU memory.
     * @param cacheKey The key used when the texture was created.
     * @param device   Vulkan logical device.
     */
    void evictTexture(const std::string& cacheKey, VkDevice device);

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
     * @brief Helper to configure texture samplers with the specified filter mode.
     */
    void createTextureSampler(VkDevice device, Texture& texture, TextureFilterMode filterMode);
};
