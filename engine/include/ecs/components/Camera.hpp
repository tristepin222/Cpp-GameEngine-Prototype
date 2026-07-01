#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../components/Transform.hpp"

/**
 * @struct Camera
 * @brief Represents a camera component for rendering and viewing.
 */
struct Camera {
    /** @brief Field of view in degrees. */
    float fov = 45.f;
    /** @brief Aspect ratio (width / height). */
    float aspect = 1.0f;   // window width / height
    /** @brief Distance to the near clipping plane. */
    float nearPlane = 0.1f;
    /** @brief Distance to the far clipping plane. */
    float farPlane = 100.f;
    /** @brief Speed of camera movement. */
    float moveSpeed = 5.f;
    /** @brief Mouse sensitivity for rotation. */
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
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
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
