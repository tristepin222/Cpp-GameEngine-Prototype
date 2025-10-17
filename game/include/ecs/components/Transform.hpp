#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
    glm::vec3 position{ 0.0f };
    glm::quat rotation{ 1.0, 0.0, 0.0, 0.0 };
    glm::vec3 scale{ 1.0f };

    glm::mat4 worldMatrix{ 1.0f };

    // Compute the world matrix from position, rotation, scale
    glm::mat4 getMatrix() const {
        glm::mat4 mat = glm::translate(glm::mat4(1.0f), position);
        mat *= glm::mat4_cast(rotation);
        mat = glm::scale(mat, scale);
        return mat;
    }
};
