// Renderer.h (or a separate Uniforms.h)
#pragma once
#include <glm/glm.hpp>

/**
 * @struct CameraUBO
 * @brief Uniform Buffer Object containing view and projection matrices.
 */
struct CameraUBO {
    /** @brief The view matrix. */
    glm::mat4 view;
    /** @brief The projection matrix. */
    glm::mat4 proj;
};
