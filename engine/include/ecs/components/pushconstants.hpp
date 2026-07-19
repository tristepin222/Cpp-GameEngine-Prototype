#pragma once

#include <glm/glm.hpp>


/**
 * @struct PushConstants
 * @brief Struct for passing constant data directly via Vulkan push constants to shaders.
 */
struct PushConstants {
    /** @brief Model matrix of the object being rendered. */
    glm::mat4 model;
    /** @brief Base color multiplier. */
    glm::vec4 color;
    /** @brief Precomputed view-projection matrix. */
    glm::mat4 viewProj;
    /** @brief Camera position in world space. */
    glm::vec4 camPos;
    /** @brief Scale factor. */
    float scale;
    /** @brief Fade amount (e.g., for grids). */
    float fade;
};

/**
 * @struct InstanceDataGPU
 * @brief Layout of per-instance data uploaded to the GPU for instanced rendering.
 */
struct InstanceDataGPU {
    /** @brief Model transform matrix. */
    glm::mat4 model;
    /** @brief Instance color. */
    glm::vec4 color;
    /** @brief ID of the mesh to use. */
    uint32_t meshID;
    /** @brief ID of the material to use. */
    uint32_t materialID;
};