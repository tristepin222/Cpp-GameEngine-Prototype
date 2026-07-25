#pragma once

#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


/**
 * @struct PlayerControllerComponent
 * @brief Component to mark and control player movement and interaction.
 */
// [ReflectClass]
struct ENGINE_API PlayerControllerComponent {
    // [ReflectField]
    float speed = 5.0f;
    // [ReflectField]
    float jumpForce = 6.0f;
    // [ReflectField]
    float interactRange = 3.0f;

    // Configuration flags
    // [ReflectField]
    bool orientToMovement = true;


    // Transient input state
    bool wasJumpPressed = false;
    bool wasInteractPressed = false;

    // Diagnostic state
    int debugRunningCount = 0;
    glm::vec3 debugRbVelocity{0.0f};
    float debugMoveDirLength = 0.0f;
};

REGISTER_COMPONENT(PlayerControllerComponent, "Gameplay");







