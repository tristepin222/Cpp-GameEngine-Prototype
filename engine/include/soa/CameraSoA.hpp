#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct CameraSoA {
    std::vector<float> fovs;
    std::vector<float> aspects;
    std::vector<float> nearPlanes;
    std::vector<float> farPlanes;

    std::vector<glm::mat4> viewProjMatrices; // output for GPU

    void clear() {
        fovs.clear();
        aspects.clear();
        nearPlanes.clear();
        farPlanes.clear();
        viewProjMatrices.clear();
    }

    size_t pushCamera(float fov, float aspect, float nearPlane, float farPlane) {
        fovs.push_back(fov);
        aspects.push_back(aspect);
        nearPlanes.push_back(nearPlane);
        farPlanes.push_back(farPlane);
        viewProjMatrices.emplace_back(1.0f); // placeholder
        return fovs.size() - 1;
    }

    size_t size() const { return fovs.size(); }

    // Optimized: aligned with TransformSoA (camera i == transform i)
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
