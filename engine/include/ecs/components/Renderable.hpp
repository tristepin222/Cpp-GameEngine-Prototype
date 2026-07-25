#pragma once
#include <glm/glm.hpp>

/**
 * @struct Renderable
 * @brief Component that associates an entity with a mesh and material for rendering.
 */
// [ReflectClass]
struct Renderable {
    // [ReflectField]
    uint32_t meshID;
    // [ReflectField]
    uint32_t materialID;
};


#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Renderable, "Rendering & Lights");