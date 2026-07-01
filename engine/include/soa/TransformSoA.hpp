#pragma once
#include <vector>
#include <glm/glm.hpp>

/**
 * @struct TransformSoA
 * @brief Structure of Arrays representing translation, rotation, and scaling vectors.
 */
struct TransformSoA {
    /** @brief 3D position vectors. */
    std::vector<glm::vec3> positions;
    /** @brief 3D Euler rotation vectors in degrees. */
    std::vector<glm::vec3> rotations; // Euler angles in radians
    /** @brief 3D scale vectors. */
    std::vector<glm::vec3> scales;

    /**
     * @brief Clears all vector arrays.
     */
    void clear() {
        positions.clear();
        rotations.clear();
        scales.clear();
    }

    /**
     * @brief Appends transform component values.
     * @param pos 3D position.
     * @param rot 3D Euler rotation.
     * @param scale 3D scale.
     * @return The index of the added transform.
     */
    size_t push(const glm::vec3& pos, const glm::vec3& rot, const glm::vec3& scale = glm::vec3(1.0f)) {
        positions.push_back(pos);
        rotations.push_back(rot);
        scales.push_back(scale);
        return positions.size() - 1; // index of new transform
    }

    /**
     * @brief Gets total transform entries count.
     * @return Transform count.
     */
    size_t size() const {
        return positions.size();
    }

    /**
     * @brief Computes the model matrix for a specific index.
     * @param i Index of the transform.
     * @return 4x4 model matrix.
     */
    glm::mat4 modelMatrix(size_t i) const {
        glm::mat4 m(1.0f);
        m = glm::translate(m, positions[i]);
        m = glm::rotate(m, rotations[i].x, glm::vec3(1,0,0));
        m = glm::rotate(m, rotations[i].y, glm::vec3(0,1,0));
        m = glm::rotate(m, rotations[i].z, glm::vec3(0,0,1));
        m = glm::scale(m, scales[i]);
        return m;
    }

    /**
     * @brief Retrieves local forward direction vector for index.
     * @param i Transform index.
     * @return Forward vector.
     */
    glm::vec3 forward(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(0,0,-1));
    }

    /**
     * @brief Retrieves local up direction vector for index.
     * @param i Transform index.
     * @return Up vector.
     */
    glm::vec3 up(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(0,1,0));
    }

    /**
     * @brief Retrieves local right direction vector for index.
     * @param i Transform index.
     * @return Right vector.
     */
    glm::vec3 right(size_t i) const {
        glm::quat q = glm::quat(rotations[i]);
        return glm::normalize(q * glm::vec3(1,0,0));
    }
};
