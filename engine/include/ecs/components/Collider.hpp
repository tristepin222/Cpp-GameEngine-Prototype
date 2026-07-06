#pragma once
#include <glm/glm.hpp>

enum class ColliderShape {
    Sphere,
    AABB,
    OBB
};

/**
 * @struct ColliderComponent
 * @brief Represents a collision volume (Sphere or Axis-Aligned Bounding Box).
 */
struct ColliderComponent {
    ColliderShape shape = ColliderShape::AABB;
    float radius = 1.0f;                       // Used for Sphere colliders
    glm::vec3 extents = glm::vec3(0.5f);       // Half-extents for AABB colliders
    glm::vec3 offset = glm::vec3(0.0f);        // Local position offset relative to transform
};
