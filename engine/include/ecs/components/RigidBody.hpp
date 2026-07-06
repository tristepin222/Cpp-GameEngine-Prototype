#pragma once
#include <glm/glm.hpp>

enum class RigidBodyType {
    Dynamic,
    Static
};

/**
 * @struct RigidBodyComponent
 * @brief Holds mass, velocity, acceleration, forces, restitution, bounciness, and gravity settings.
 */
struct RigidBodyComponent {
    RigidBodyType type = RigidBodyType::Dynamic;
    float mass = 1.0f;
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 acceleration = glm::vec3(0.0f);
    glm::vec3 force = glm::vec3(0.0f);
    float gravityScale = 1.0f;
    float restitution = 0.5f; // Bounciness coefficient
};
