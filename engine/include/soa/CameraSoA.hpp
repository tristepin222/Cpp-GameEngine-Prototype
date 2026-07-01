#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * @struct CameraSoA
 * @brief Structure of Arrays representing camera components for parallel view projection calculations.
 */
struct CameraSoA {
    /** @brief Fields of view in degrees. */
    std::vector<float> fovs;
    /** @brief Viewport aspect ratios. */
    std::vector<float> aspects;
    /** @brief Distances to near clipping planes. */
    std::vector<float> nearPlanes;
    /** @brief Distances to far clipping planes. */
    std::vector<float> farPlanes;

    /** @brief Computed view-projection matrices ready for GPU transfer. */
    std::vector<glm::mat4> viewProjMatrices; // output for GPU

    /**
     * @brief Clears all arrays.
     */
    void clear() {
        fovs.clear();
        aspects.clear();
        nearPlanes.clear();
        farPlanes.clear();
        viewProjMatrices.clear();
    }

    /**
     * @brief Adds camera components to SoA.
     * @param fov Field of view.
     * @param aspect Aspect ratio.
     * @param nearPlane Near plane.
     * @param farPlane Far plane.
     * @return Index of the new camera data.
     */
    size_t pushCamera(float fov, float aspect, float nearPlane, float farPlane) {
        fovs.push_back(fov);
        aspects.push_back(aspect);
        nearPlanes.push_back(nearPlane);
        farPlanes.push_back(farPlane);
        viewProjMatrices.emplace_back(1.0f); // placeholder
        return fovs.size() - 1;
    }

    /**
     * @brief Returns total camera count.
     * @return Camera count.
     */
    size_t size() const { return fovs.size(); }

    /**
     * @brief Batch updates view projection matrices.
     * @tparam TransformSoAType Class representing transform array.
     * @param transforms The transform list matching camera indexes.
     */
    template<typename TransformSoAType>
    void updateViewProjections(const TransformSoAType& transforms) {
        size_t n = size();
        viewProjMatrices.resize(n); // ensure enough space

        // Linear memory access -> cache-friendly
        for (size_t i = 0; i < n; ++i) {
            const glm::vec3& pos = transforms.positions[i];
            const glm::vec3& rot = transforms.rotations[i];

            float pitch = glm::radians(rot.x);
            float yaw = glm::radians(rot.y);

            glm::vec3 forward;
            forward.x = cos(pitch) * cos(yaw);
            forward.y = sin(pitch);
            forward.z = cos(pitch) * sin(yaw);
            forward = glm::normalize(forward);

            glm::vec3 up(0.f, 1.f, 0.f);

            glm::mat4 view = glm::lookAt(pos, pos + forward, up);
            glm::mat4 proj = glm::perspective(glm::radians(fovs[i]), aspects[i], nearPlanes[i], farPlanes[i]);
            proj[1][1] *= -1; // Vulkan Y flip

            viewProjMatrices[i] = proj * view;
        }
    }
};
