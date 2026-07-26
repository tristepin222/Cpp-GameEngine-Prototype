#pragma once
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


namespace Engine {

    /**
     * @enum LightType
     * @brief Type of light source calculation.
     */
    enum class LightType {
        /** @brief Global directional light (e.g., Sun). */
        Directional = 0,
        /** @brief Omnidirectional point light source. */
        Point = 1,
        /** @brief Cone-constrained spot light source. */
        Spot = 2
    };

    /**
     * @struct LightComponent
     * @brief Component representing a light source in the scene.
     */
    // [ReflectClass]
    struct ENGINE_API LightComponent {
        /** @brief Type classification of the light source. */
        LightType type = LightType::Directional;
        /** @brief RGB light color vector. */
        // [ReflectField]
        glm::vec3 color{ 1.0f, 1.0f, 1.0f };
        /** @brief Intensity multiplier for brightness. */
        // [ReflectField]
        float intensity = 1.0f;
        /** @brief Attenuation distance range for point and spot lights. */
        // [ReflectField]
        float range = 10.0f; 
    };


} // namespace Engine

REGISTER_COMPONENT(Engine::LightComponent, "Rendering & Lights/Light");



