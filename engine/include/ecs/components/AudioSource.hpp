#pragma once
#include <string>
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"
#include "meta/ComponentReflection.hpp"


/**
 * @struct AudioSourceComponent
 * @brief Component to emit 2D or spatialized 3D sounds.
 */
// [ReflectClass]
struct ENGINE_API AudioSourceComponent {
    // [ReflectField]
    std::string clipPath = "";
    // [ReflectField]
    float volume = 1.0f;
    // [ReflectField]
    float pitch = 1.0f;
    // [ReflectField]
    bool loop = false;
    // [ReflectField]
    bool playOnAwake = true;
    // [ReflectField]
    bool spatialized = true;
    // [ReflectField]
    float minDistance = 1.0f;
    // [ReflectField]
    float maxDistance = 50.0f;


    // Runtime state (non-serialized)
    bool isPlaying = false;
    bool wasPlaying = false;
    void* internalSound = nullptr; // Pointer to ma_sound
    std::string currentLoadedPath = ""; // To track if we need to reload the clip
};

REGISTER_COMPONENT(AudioSourceComponent, "Audio");








