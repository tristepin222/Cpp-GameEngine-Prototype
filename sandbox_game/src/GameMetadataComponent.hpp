#pragma once
#include <string>

/**
 * @struct GameMetadataComponent
 * @brief Component containing gameplay metadata for sandbox entities.
 */
struct GameMetadataComponent {
    /** @brief Importance value used to prioritize or classify entities. */
    float importance = 42.0f;
    /** @brief Tag string identifying the entity role or type. */
    std::string tag = "Hero";
};
