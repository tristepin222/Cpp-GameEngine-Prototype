#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "../components/Transform.hpp"
#include "../Entity.hpp"
#include "../ComponentBase.hpp"

struct Camera : ComponentBase {
    float fov = 45.f;
    float aspect = 1.0f;   // window width / height
    float nearPlane = 0.1f;
    float farPlane = 100.f;
    float moveSpeed = 5.f;
    float mouseSensitivity = 0.1f;

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

    glm::mat4 projection() const {
        glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        proj[1][1] *= -1; // Vulkan's inverted Y
        return proj;
    }

    glm::mat4 viewProjection(const Transform& transform) const {
        return projection() * view(transform);
    }
};
