#pragma once
#include <glm/glm.hpp>

/**
 * @struct Renderable
 * @brief Component that associates an entity with a mesh and material for rendering.
 */
struct Renderable {
    /** @brief ID of the mesh resource. */
    uint32_t meshID;
    /** @brief ID of the material resource. */
    uint32_t materialID;
};