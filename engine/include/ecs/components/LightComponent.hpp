#pragma once
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


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

REFLECT_COMPONENT(Engine::LightComponent, "Rendering & Lights", [](Engine::ComponentReflection& refl) {
    REFLECT_FIELD(Engine::LightComponent, color);
    REFLECT_FIELD(Engine::LightComponent, intensity);
    REFLECT_FIELD(Engine::LightComponent, range);
});



