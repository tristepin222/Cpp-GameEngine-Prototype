#pragma once
#include "ecs/Entity.hpp"

#include <glm/glm.hpp>

namespace Engine {

    struct PhysgunScript {
        float Kp = 450.0f;
        float Kd = 25.0f;
        float holdDistance = 5.0f;

        // Diagnostic fields exposed in the inspector
        bool isHolding = false;
        float currentHoldDistance = 0.0f;
        bool debugShowRay = false;
        Entity originEntity;

        glm::vec3 rayOrigin{ 0.0f };
        glm::vec3 rayDirection{ 0.0f, 0.0f, -1.0f };

        int updateCount = 0;

        // Internal non-reflected state
        Entity heldEntity;
    };

} // namespace Engine
