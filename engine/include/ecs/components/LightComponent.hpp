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
    // [ReflectClass]
    struct ENGINE_API LightComponent {
        LightType type = LightType::Directional;
        // [ReflectField]
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        // [ReflectField]
        float intensity = 1.0f;
        // [ReflectField]
        float range = 10.0f; 
    };

} // namespace Engine

REGISTER_COMPONENT(Engine::LightComponent, "Rendering & Lights");



