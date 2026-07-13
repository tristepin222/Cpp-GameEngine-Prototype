#pragma once
#include <glm/glm.hpp>

#include "core/EngineAPI.hpp"

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
struct ENGINE_API ColliderComponent {
    ColliderShape shape = ColliderShape::AABB;
    float radius = 1.0f;                       // Used for Sphere and Capsule colliders
    float height = 2.0f;                       // Total height (including caps) for Capsule colliders
    glm::vec3 extents = glm::vec3(0.5f);       // Half-extents for AABB/OBB colliders
    glm::vec3 offset = glm::vec3(0.0f);        // Local position offset relative to transform
};
