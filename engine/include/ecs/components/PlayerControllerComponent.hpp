#pragma once

#include "core/EngineAPI.hpp"

/**
 * @struct PlayerControllerComponent
 * @brief Component to mark and control player movement and interaction.
 */
// @reflect
struct ENGINE_API PlayerControllerComponent {
    // @reflect
    float speed = 5.0f;
    // @reflect
    float jumpForce = 6.0f;
    // @reflect
    float interactRange = 3.0f;

    // Configuration flags
    // @reflect
    bool orientToMovement = true;

    // Transient input state
    bool wasJumpPressed = false;
    bool wasInteractPressed = false;

    // Diagnostic state
    int debugRunningCount = 0;
    glm::vec3 debugRbVelocity{0.0f};
    float debugMoveDirLength = 0.0f;
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(PlayerControllerComponent, "Gameplay");


