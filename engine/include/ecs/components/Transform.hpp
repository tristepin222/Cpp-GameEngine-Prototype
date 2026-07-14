#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <initializer_list>
#include "core/EngineAPI.hpp"

/**
 * @struct RotationField
 * @brief Compatibility layer that wraps a glm::quat but behaves like a glm::vec3 for Euler angles.
 */
struct ENGINE_API RotationField {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

private:
    glm::quat q{ 1.0f, 0.0f, 0.0f, 0.0f };
    mutable float lastX = 0.0f;
    mutable float lastY = 0.0f;
    mutable float lastZ = 0.0f;

    void syncQuatFromEuler() const {
        if (x != lastX || y != lastY || z != lastZ) {
            glm::quat qx = glm::angleAxis(glm::radians(x), glm::vec3(1.0f, 0.0f, 0.0f));
            glm::quat qy = glm::angleAxis(glm::radians(y), glm::vec3(0.0f, 1.0f, 0.0f));
            glm::quat qz = glm::angleAxis(glm::radians(z), glm::vec3(0.0f, 0.0f, 1.0f));
            const_cast<glm::quat&>(q) = glm::normalize(qz * qy * qx);
            lastX = x;
            lastY = y;
            lastZ = z;
        }
    }

    void syncEulerFromQuat() {
        glm::mat3 R = glm::mat3_cast(q);
        float thetaX = 0.0f;
        float thetaY = 0.0f;
        float thetaZ = 0.0f;

        float sinY = -R[0][2];
        if (sinY < 0.9999f && sinY > -0.9999f) {
            thetaY = std::asin(sinY);
            thetaX = std::atan2(R[1][2], R[2][2]);
            thetaZ = std::atan2(R[0][1], R[0][0]);
        } else {
            thetaY = (sinY >= 0.0f) ? (3.14159265f * 0.5f) : (-3.14159265f * 0.5f);
            thetaX = 0.0f;
            thetaZ = std::atan2(-R[1][0], R[1][1]);
        }

        x = glm::degrees(thetaX);
        y = glm::degrees(thetaY);
        z = glm::degrees(thetaZ);
        lastX = x;
        lastY = y;
        lastZ = z;
    }

public:
    RotationField() = default;

    RotationField(const glm::vec3& euler) {
        x = euler.x; y = euler.y; z = euler.z;
        syncQuatFromEuler();
    }

    RotationField(const glm::quat& quaternion) {
        q = glm::normalize(quaternion);
        syncEulerFromQuat();
    }

    RotationField& operator=(const glm::vec3& eulerDegrees) {
        x = eulerDegrees.x;
        y = eulerDegrees.y;
        z = eulerDegrees.z;
        syncQuatFromEuler();
        return *this;
    }

    RotationField& operator=(const glm::quat& newQ) {
        q = glm::normalize(newQ);
        syncEulerFromQuat();
        return *this;
    }

    RotationField& operator=(const std::initializer_list<float>& list) {
        auto it = list.begin();
        if (it != list.end()) x = *it++;
        if (it != list.end()) y = *it++;
        if (it != list.end()) z = *it++;
        syncQuatFromEuler();
        return *this;
    }

    operator glm::vec3() const {
        return glm::vec3(x, y, z);
    }

    operator glm::quat() const {
        syncQuatFromEuler();
        return q;
    }

    glm::quat getQuat() const {
        syncQuatFromEuler();
        return q;
    }

    void setQuat(const glm::quat& newQ) {
        q = glm::normalize(newQ);
        syncEulerFromQuat();
    }
};

/**
 * @struct Transform
 * @brief Represents the position, rotation, and scale of an entity in 3D space.
 */
struct ENGINE_API Transform {

    /** @brief Position of the entity in 3D space. */
    glm::vec3 position{ 0.0f };
    /** @brief Rotation of the entity stored natively as a quaternion compatibility field. */
    RotationField rotation;
    /** @brief Scale of the entity in 3D space. */
    glm::vec3 scale{ 1.0f };

    /**
     * @brief Converts the Euler rotation angles to a quaternion.
     * @return The rotation quaternion.
     */
    glm::quat rotationQuat() const {
        return rotation.getQuat();
    }

    /**
     * @brief Gets the forward direction vector in local space.
     * @return Normalized forward vector.
     */
    glm::vec3 forward() const {
        return glm::normalize(rotation.getQuat() * glm::vec3(0, 0, -1));
    }

    /**
     * @brief Gets the right direction vector in local space.
     * @return Normalized right vector.
     */
    glm::vec3 right() const {
        return glm::normalize(rotation.getQuat() * glm::vec3(1, 0, 0));
    }

    /**
     * @brief Gets the up direction vector in local space.
     * @return Normalized up vector.
     */
    glm::vec3 up() const {
        return glm::normalize(rotation.getQuat() * glm::vec3(0, 1, 0));
    }

    /**
     * @brief Computes the model matrix representing the transform.
     * @return The 4x4 transform matrix.
     */
    glm::mat4 matrix() const
    {
        glm::mat4 m(1.0f);
        m = glm::translate(m, position);
        m = m * glm::mat4_cast(rotation.getQuat());
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
