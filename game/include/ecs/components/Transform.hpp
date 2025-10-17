#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../ComponentBase.hpp"

struct Transform : ComponentBase {
    glm::vec3 position{ 0.0f };
    glm::vec3 rotation{ 0.0f }; // pitch(x), yaw(y), roll(z) in radians
    glm::vec3 scale{ 1.0f };

    // Convert rotation to a quaternion for easier direction calculation
    glm::quat rotationQuat() const {
        return glm::quat(glm::vec3(rotation.x, rotation.y, rotation.z));
    }

    // Local axes
    glm::vec3 forward() const {
        return glm::normalize(rotationQuat() * glm::vec3(0, 0, -1));
    }

    glm::vec3 right() const {
        return glm::normalize(rotationQuat() * glm::vec3(1, 0, 0));
    }

    glm::vec3 up() const {
        return glm::normalize(rotationQuat() * glm::vec3(0, 1, 0));
    }

    // Model matrix
    glm::mat4 matrix() const {
        glm::mat4 m{ 1.0f };
        m = glm::translate(m, position);
        m = glm::rotate(m, rotation.x, glm::vec3(1, 0, 0));
        m = glm::rotate(m, rotation.y, glm::vec3(0, 1, 0));
        m = glm::rotate(m, rotation.z, glm::vec3(0, 0, 1));
        m = glm::scale(m, scale);
        return m;
    }

    Transform() = default;

    Transform(glm::vec3 pos, glm::vec3 rot = glm::vec3(0.f), glm::vec3 s = glm::vec3(1.f))
        : position(pos), rotation(rot), scale(s) {
    }
};
