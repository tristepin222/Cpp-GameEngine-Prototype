#pragma once
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"

namespace Engine {

    enum class LightType {
        Directional = 0,
        Point = 1,
        Spot = 2
    };

    /**
     * @struct LightComponent
     * @brief Component representing a light source in the scene.
     */
    struct ENGINE_API LightComponent {
        LightType type = LightType::Directional;
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        float intensity = 1.0f;
        float range = 10.0f;
    };

}
