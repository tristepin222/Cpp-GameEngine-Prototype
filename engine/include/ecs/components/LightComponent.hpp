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
    // @reflect
    struct ENGINE_API LightComponent {
        LightType type = LightType::Directional;
        // @reflect
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        // @reflect
        float intensity = 1.0f;
        // @reflect
        float range = 10.0f;
    };


}

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Engine::LightComponent, "Rendering & Lights");

