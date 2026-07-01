#pragma once
#include <glm/glm.hpp>

/**
 * @struct InstanceData
 * @brief Represents per-instance model data for rendering.
 */
struct InstanceData {
    /** @brief Model transform matrix of the instance. */
    glm::mat4 model;
};
