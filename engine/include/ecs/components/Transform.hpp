#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
/**
 * @struct Transform
 * @brief Represents the position, rotation, and scale of an entity in 3D space.
 */
struct Transform {

    /** @brief Position of the entity in 3D space. */
    glm::vec3 position{ 0.0f };
    /** @brief Rotation of the entity (pitch, yaw, roll) in degrees. */
    glm::vec3 rotation{ 0.0f }; // pitch(x), yaw(y), roll(z) in radians
    /** @brief Scale of the entity in 3D space. */
    glm::vec3 scale{ 1.0f };

    /**
     * @brief Converts the Euler rotation angles to a quaternion.
     * @return The rotation quaternion.
     */
    glm::quat rotationQuat() const {
        return glm::quat(glm::vec3(rotation.x, rotation.y, rotation.z));
    }

    /**
     * @brief Gets the forward direction vector in local space.
     * @return Normalized forward vector.
     */
    glm::vec3 forward() const {
        return glm::normalize(rotationQuat() * glm::vec3(0, 0, -1));
    }

    /**
     * @brief Gets the right direction vector in local space.
     * @return Normalized right vector.
     */
    glm::vec3 right() const {
        return glm::normalize(rotationQuat() * glm::vec3(1, 0, 0));
    }

    /**
     * @brief Gets the up direction vector in local space.
     * @return Normalized up vector.
     */
    glm::vec3 up() const {
        return glm::normalize(rotationQuat() * glm::vec3(0, 1, 0));
    }

    /**
     * @brief Computes the model matrix representing the transform.
     * @return The 4x4 transform matrix.
     */
    glm::mat4 matrix() const
    {
        glm::mat4 m(1.0f);

        m = glm::translate(m, position);

        m = glm::rotate(m, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(rotation.z), glm::vec3(0, 0, 1));

        m = glm::scale(m, scale);

        return m;
    }
    /**
     * @brief Default constructor for Transform.
     */
    Transform() = default;

    /**
     * @brief Construct a new Transform object.
     * @param pos Initial position.
     * @param rot Initial rotation.
     * @param s Initial scale.
     */
    Transform(glm::vec3 pos, glm::vec3 rot = glm::vec3(0.f), glm::vec3 s = glm::vec3(1.f))
        : position(pos), rotation(rot), scale(s) {
    }
};
