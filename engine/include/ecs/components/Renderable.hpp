#pragma once
#include <glm/glm.hpp>

/**
 * @struct Renderable
 * @brief Component that associates an entity with a mesh and material for rendering.
 */
// @reflect
struct Renderable {
    // @reflect
    uint32_t meshID;
    // @reflect
    uint32_t materialID;
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Renderable, "Rendering & Lights");