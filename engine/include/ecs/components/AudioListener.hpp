#pragma once
#include "core/EngineAPI.hpp"

/**
 * @struct AudioListenerComponent
 * @brief Component representing a 3D audio listener.
 */
// @reflect
struct ENGINE_API AudioListenerComponent {
    // @reflect
    bool active = true;
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(AudioListenerComponent, "Audio");


