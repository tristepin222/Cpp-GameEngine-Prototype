#pragma once
#include <string>
#include <glm/glm.hpp>
#include "core/EngineAPI.hpp"

/**
 * @struct AudioSourceComponent
 * @brief Component to emit 2D or spatialized 3D sounds.
 */
struct ENGINE_API AudioSourceComponent {
    std::string clipPath = "";
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool playOnAwake = true;
    bool spatialized = true;
    float minDistance = 1.0f;
    float maxDistance = 50.0f;

    // Runtime state (non-serialized)
    bool isPlaying = false;
    bool wasPlaying = false;
    void* internalSound = nullptr; // Pointer to ma_sound
    std::string currentLoadedPath = ""; // To track if we need to reload the clip
};
