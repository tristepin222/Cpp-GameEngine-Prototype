#pragma once
#include <glm/glm.hpp>

#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


enum class ColliderShape {
    Sphere,
    AABB,
    OBB,
    Capsule
};

/**
 * @struct ColliderComponent
 * @brief Represents a collision volume (Sphere, Axis-Aligned Bounding Box, Oriented Bounding Box, or Capsule).
 */
// @reflect
struct ENGINE_API ColliderComponent {
    ColliderShape shape = ColliderShape::AABB;
    // @reflect
    float radius = 1.0f;                       // Used for Sphere and Capsule colliders
    // @reflect
    float height = 2.0f;                       // Total height (including caps) for Capsule colliders
    // @reflect
    glm::vec3 extents = glm::vec3(0.5f);       // Half-extents for AABB/OBB colliders
    // @reflect
    glm::vec3 offset = glm::vec3(0.0f);        // Local position offset relative to transform
};

REFLECT_COMPONENT(ColliderComponent, "Physics", [](Engine::ComponentReflection& refl) {
    REFLECT_FIELD(ColliderComponent, radius);
    REFLECT_FIELD(ColliderComponent, height);
    REFLECT_FIELD(ColliderComponent, extents);
    REFLECT_FIELD(ColliderComponent, offset);
});




