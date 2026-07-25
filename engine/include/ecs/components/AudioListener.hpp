#pragma once
#include "core/EngineAPI.hpp"

/**
 * @struct AudioListenerComponent
 * @brief Component representing a 3D audio listener.
 */
// [ReflectClass]
struct ENGINE_API AudioListenerComponent {
    // [ReflectField]
    bool active = true;
};


#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(AudioListenerComponent, "Audio");


