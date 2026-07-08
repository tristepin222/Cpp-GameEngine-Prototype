#pragma once
#include "ecs/Entity.hpp"
#include <glm/glm.hpp>

/**
 * @struct CinemachineVirtualCamera
 * @brief Addon component that represents a virtual camera.
 *        Managed by CinemachineSystem to calculate tracking offsets, damping, and priority-blending.
 */
struct CinemachineVirtualCamera {
    Entity followTarget = Entity();
    Entity lookAtTarget = Entity();

    glm::vec3 followOffset = glm::vec3(0.0f, 4.0f, 8.0f);
    float followDamping = 2.0f; // Damping rate (higher = slower, 0.0f = instant)
    float lookAtDamping = 1.0f; // Damping rate for rotation

    float fov = 45.0f;
    int priority = 10;
    bool active = true;

    // Internal tracking positions (cached to smooth out from frame to frame)
    glm::vec3 currentPosition = glm::vec3(0.0f);
    glm::vec3 currentRotationEuler = glm::vec3(0.0f);
    bool initialized = false;
};
