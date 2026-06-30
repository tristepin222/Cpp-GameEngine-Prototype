#pragma once
#include <vector>
#include <glm/glm.hpp>

struct TransformSoA {
    // SoA fields
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> rotations; // Euler angles in radians
    std::vector<glm::vec3> scales;

    // --- Methods ---
    void clear() {
        positions.clear();
        rotations.clear();
        scales.clear();
    }

    size_t push(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale = glm::vec3(1.0f)) {
        positions.push_back(pos);
        rotations.push_back(rot);
        scales.push_back(scale);
        return positions.size() - 1; // index of new transform
    }

    size_t size() const {
        return positions.size();
    }

    // Generate model matrix for a given index
    glm::mat4 modelMatrix(size_t i) const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, positions[i]);
        m = glm::rotate(m, rotations[i].x, glm::vec3(1,0,0));
        m = glm::rotate(m, rotations[i].y, glm::vec3(0,1,0));
        m = glm::rotate(m, rotations[i].z, glm::vec3(0,0,1));
        m = glm::scale(m, scales[i]);
        return m;
    }

    // Access helpers
    glm::vec3 forward(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(0,0,-1));
    }

    glm::vec3 up(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(0,1,0));
    }

    glm::vec3 right(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(1,0,0));
    }
};
