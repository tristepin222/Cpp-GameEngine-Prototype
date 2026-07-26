#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../components/Transform.hpp"
#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


/**
 * @struct Camera
 * @brief Represents a camera component for rendering and viewing.
 */
// [ReflectClass]
struct ENGINE_API Camera {
    // [ReflectField]
    bool isOrthographic = false;
    // [ReflectField]
    float orthoSize = 5.0f;
    // [ReflectField]
    float fov = 45.f;
    // [ReflectField]
    float aspect = 1.0f;   // window width / height
    // [ReflectField]
    float nearPlane = 0.1f;
    // [ReflectField]
    float farPlane = 100.f;

    float moveSpeed = 5.f;
    float mouseSensitivity = 0.1f;


    /**
     * @brief Calculates the view matrix based on the given transform.
     * @param transform The transform of the camera entity.
     * @return The 4x4 view matrix.
     */
    glm::mat4 view(const Transform& transform) const {
        // Convert Euler rotation (pitch, yaw, roll) to direction
        float pitch = glm::radians(transform.rotation.x);
        float yaw = glm::radians(transform.rotation.y);

        glm::vec3 forward;
        forward.x = cos(pitch) * cos(yaw);
        forward.y = sin(pitch);
        forward.z = cos(pitch) * sin(yaw);
        forward = glm::normalize(forward);

        glm::vec3 up(0.f, 1.f, 0.f);
        return glm::lookAt(transform.position, transform.position + forward, up);
    }

    /**
     * @brief Calculates the projection matrix.
     * @return The 4x4 projection matrix.
     */
    glm::mat4 projection() const {
        glm::mat4 proj;
        if (isOrthographic) {
            float halfWidth = orthoSize * aspect;
            float halfHeight = orthoSize;
            float actualNear = (nearPlane > 0.0f) ? -farPlane : nearPlane;
            proj = glm::orthoRH_ZO(-halfWidth, halfWidth, -halfHeight, halfHeight, actualNear, farPlane);
        } else {
            proj = glm::perspectiveRH_ZO(glm::radians(fov), aspect, nearPlane, farPlane);
        }
        proj[1][1] *= -1; // Vulkan's inverted Y
        return proj;
    }



    /**
     * @brief Calculates the view-projection matrix.
     * @param transform The transform of the camera entity.
     * @return The 4x4 view-projection matrix.
     */
    glm::mat4 viewProjection(const Transform& transform) const {
        return projection() * view(transform);
    }
};

REGISTER_COMPONENT(Camera, "Rendering & Lights");







