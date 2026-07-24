#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <string>

enum class TextureFilterMode {
    Nearest,
    Bilinear,
    Trilinear
};

/**
 * @struct Material
 * @brief Represents a material component defining rendering properties.
 */
// @reflect
struct Material {

    /** @brief Unique identifier for this material. */
    uint32_t id;

    // @reflect
    glm::vec4 color{ 1.0f }; // RGBA
    // @reflect
    std::string texturePath;
    /** @brief Vulkan descriptor set representing resource bindings. */
    VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
    /** @brief Optional per-material custom Vulkan pipeline. */
    VkPipeline pipeline{ VK_NULL_HANDLE }; // optional per-shader pipeline
    /** @brief Optional per-material custom Vulkan pipeline layout. */
    VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE }; // optional per-shader pipeline layout
    /** @brief Texture filter mode. */
    TextureFilterMode filterMode = TextureFilterMode::Bilinear;

    // Shader and mapping properties
    // @reflect
    std::string shaderName = "Unlit";
    // @reflect
    std::string normalMapPath;
    // @reflect
    std::string metallicMapPath;
    // @reflect
    float roughness = 0.5f;
    // @reflect
    float metallic = 0.0f;


    /**
     * @brief Construct a new Material object.
     * @param c Color of the material.
     * @param ds Descriptor set associated with this material.
     * @param pp Custom graphics pipeline.
     */
    Material(const glm::vec4& c = { 1.f,1.f,1.f,1.f }, VkDescriptorSet ds = VK_NULL_HANDLE, VkPipeline pp = VK_NULL_HANDLE)
        : color(c), descriptorSet(ds) {
    }
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Material, "Rendering & Lights");

