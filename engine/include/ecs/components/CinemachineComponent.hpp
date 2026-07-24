#pragma once
#include "ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <string>
#include "core/EngineAPI.hpp"

enum class CinemachineMode {
    ThirdPersonFollow = 0,
    FirstPerson = 1,
    FixedLookAt = 2
};

/**
 * @struct CinemachineVirtualCamera
 * @brief Addon component that represents a virtual camera.
 *        Managed by CinemachineSystem to calculate tracking offsets, damping, and priority-blending.
 */
// @reflect
struct CinemachineVirtualCamera {
    Entity followTarget = Entity();
    Entity lookAtTarget = Entity();

    // @reflect
    std::string followTargetName;
    // @reflect
    std::string lookAtTargetName;
    // @reflect
    std::string lockToBone = "Head";

    CinemachineMode mode = CinemachineMode::ThirdPersonFollow;
    // @reflect
    bool mouseOrbit = true;
    // @reflect
    bool mouseLook = true;
    // @reflect
    float orbitSensitivity = 0.1f;
    // @reflect
    float orbitYaw = 0.0f;
    // @reflect
    float orbitPitch = 0.0f;
    // @reflect
    float cameraYaw = 0.0f;
    // @reflect
    float cameraPitch = 0.0f;

    // @reflect
    glm::vec3 followOffset = glm::vec3(0.0f, 4.0f, 8.0f);
    // @reflect
    float followDamping = 2.0f; // Damping rate (higher = slower, 0.0f = instant)
    // @reflect
    float lookAtDamping = 1.0f; // Damping rate for rotation

    // @reflect
    float fov = 45.0f;
    // @reflect
    int priority = 10;
    // @reflect
    bool active = true;

    // Internal tracking positions (cached to smooth out from frame to frame)
    glm::vec3 currentPosition = glm::vec3(0.0f);
    glm::vec3 currentRotationEuler = glm::vec3(0.0f);
    bool initialized = false;
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(CinemachineVirtualCamera, "Rendering & Lights");

