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
// [ReflectClass]
struct CinemachineVirtualCamera {
    Entity followTarget = Entity();
    Entity lookAtTarget = Entity();

    // [ReflectField]
    std::string followTargetName;
    // [ReflectField]
    std::string lookAtTargetName;
    // [ReflectField]
    std::string lockToBone = "Head";

    CinemachineMode mode = CinemachineMode::ThirdPersonFollow;
    // [ReflectField]
    bool mouseOrbit = true;
    // [ReflectField]
    bool mouseLook = true;
    // [ReflectField]
    float orbitSensitivity = 0.1f;
    // [ReflectField]
    float orbitYaw = 0.0f;
    // [ReflectField]
    float orbitPitch = 0.0f;
    // [ReflectField]
    float cameraYaw = 0.0f;
    // [ReflectField]
    float cameraPitch = 0.0f;

    // [ReflectField]
    glm::vec3 followOffset = glm::vec3(0.0f, 4.0f, 8.0f);
    // [ReflectField]
    float followDamping = 2.0f; // Damping rate (higher = slower, 0.0f = instant)
    // [ReflectField]
    float lookAtDamping = 1.0f; // Damping rate for rotation

    // [ReflectField]
    float fov = 45.0f;
    // [ReflectField]
    int priority = 10;
    // [ReflectField]
    bool active = true;


    // Internal tracking positions (cached to smooth out from frame to frame)
    glm::vec3 currentPosition = glm::vec3(0.0f);
    glm::vec3 currentRotationEuler = glm::vec3(0.0f);
    bool initialized = false;
};


