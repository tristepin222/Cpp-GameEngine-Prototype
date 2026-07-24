#pragma once
#include <string>
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"

/**
 * @struct AudioSourceComponent
 * @brief Component to emit 2D or spatialized 3D sounds.
 */
// @reflect
struct ENGINE_API AudioSourceComponent {
    // @reflect
    std::string clipPath = "";
    // @reflect
    float volume = 1.0f;
    // @reflect
    float pitch = 1.0f;
    // @reflect
    bool loop = false;
    // @reflect
    bool playOnAwake = true;
    // @reflect
    bool spatialized = true;
    // @reflect
    float minDistance = 1.0f;
    // @reflect
    float maxDistance = 50.0f;

    // Runtime state (non-serialized)
    bool isPlaying = false;
    bool wasPlaying = false;
    void* internalSound = nullptr; // Pointer to ma_sound
    std::string currentLoadedPath = ""; // To track if we need to reload the clip
};

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(AudioSourceComponent, "Audio");


