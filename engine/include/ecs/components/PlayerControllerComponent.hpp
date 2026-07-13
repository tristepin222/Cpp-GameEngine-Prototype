#pragma once

#include "core/EngineAPI.hpp"

/**
 * @struct PlayerControllerComponent
 * @brief Component to mark and control player movement and interaction.
 */
struct ENGINE_API PlayerControllerComponent {
    float speed = 5.0f;
    float jumpForce = 6.0f;
    float interactRange = 3.0f;

    // Configuration flags
    bool orientToMovement = true;

    // Transient input state
    bool wasJumpPressed = false;
    bool wasInteractPressed = false;

    // Diagnostic state
    int debugRunningCount = 0;
    glm::vec3 debugRbVelocity{0.0f};
    float debugMoveDirLength = 0.0f;
};
