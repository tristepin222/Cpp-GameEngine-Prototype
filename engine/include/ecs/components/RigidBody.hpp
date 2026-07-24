#pragma once
#include <glm/glm.hpp>

#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


enum class RigidBodyType {
    Dynamic,
    Static
};

/**
 * @struct RigidBodyComponent
 * @brief Holds mass, velocity, acceleration, forces, restitution, bounciness, and gravity settings.
 */
// @reflect
struct ENGINE_API RigidBodyComponent {
    RigidBodyType type = RigidBodyType::Dynamic;
    // @reflect
    float mass = 1.0f;
    // @reflect
    glm::vec3 velocity = glm::vec3(0.0f);
    // @reflect
    glm::vec3 acceleration = glm::vec3(0.0f);
    // @reflect
    glm::vec3 force = glm::vec3(0.0f);
    // @reflect
    float gravityScale = 1.0f;
    // @reflect
    float restitution = 0.5f; // Bounciness coefficient
    // @reflect
    float friction = 0.3f;    // Sliding friction coefficient

    // Rotational physics fields
    // @reflect
    glm::vec3 angularVelocity = glm::vec3(0.0f); // Rad/s
    // @reflect
    glm::vec3 torque = glm::vec3(0.0f);
    // @reflect
    float angularDrag = 0.5f; // Damping
    // @reflect
    float linearDrag = 0.0f;  // Damping

    // Sleep system
    float sleepTimer           = 0.0f;   // Accumulated time below sleep threshold
    bool  sleeping             = false;  // True when body is fully at rest
    bool  hadContactThisFrame  = false;  // Set by collision resolver; cleared each integration pass
                                         // Used to prevent free-falling bodies from sleeping
    bool  unstableContactThisFrame = false; // Contact support is producing torque; don't sleep yet

    // Constraints (Freeze axes)
    // @reflect
    bool freezePositionX = false;
    // @reflect
    bool freezePositionY = false;
    // @reflect
    bool freezePositionZ = false;
    // @reflect
    bool freezeRotationX = false;
    // @reflect
    bool freezeRotationY = false;
    // @reflect
    bool freezeRotationZ = false;
};

REFLECT_COMPONENT(RigidBodyComponent, "Physics", [](Engine::ComponentReflection& refl) {
    REFLECT_FIELD(RigidBodyComponent, type);
    REFLECT_FIELD(RigidBodyComponent, mass);
    REFLECT_FIELD(RigidBodyComponent, velocity);
    REFLECT_FIELD(RigidBodyComponent, acceleration);
    REFLECT_FIELD(RigidBodyComponent, force);
    REFLECT_FIELD(RigidBodyComponent, gravityScale);
    REFLECT_FIELD(RigidBodyComponent, restitution);
    REFLECT_FIELD(RigidBodyComponent, friction);
    REFLECT_FIELD(RigidBodyComponent, angularVelocity);
    REFLECT_FIELD(RigidBodyComponent, torque);
    REFLECT_FIELD(RigidBodyComponent, angularDrag);
    REFLECT_FIELD(RigidBodyComponent, linearDrag);
    REFLECT_FIELD(RigidBodyComponent, freezePositionX);
    REFLECT_FIELD(RigidBodyComponent, freezePositionY);
    REFLECT_FIELD(RigidBodyComponent, freezePositionZ);
    REFLECT_FIELD(RigidBodyComponent, freezeRotationX);
    REFLECT_FIELD(RigidBodyComponent, freezeRotationY);
    REFLECT_FIELD(RigidBodyComponent, freezeRotationZ);
});



