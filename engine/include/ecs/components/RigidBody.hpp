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
    float friction = 0.3f;    // Sliding friction coefficient

    // Rotational physics fields
    glm::vec3 angularVelocity = glm::vec3(0.0f); // Rad/s
    glm::vec3 torque = glm::vec3(0.0f);
    float angularDrag = 0.5f; // Damping
    float linearDrag = 0.0f;  // Damping

    // Sleep system
    float sleepTimer           = 0.0f;   // Accumulated time below sleep threshold
    bool  sleeping             = false;  // True when body is fully at rest
    bool  hadContactThisFrame  = false;  // Set by collision resolver; cleared each integration pass
                                         // Used to prevent free-falling bodies from sleeping
    bool  unstableContactThisFrame = false; // Contact support is producing torque; don't sleep yet
};
