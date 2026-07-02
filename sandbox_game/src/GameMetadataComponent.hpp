#pragma once
#include <string>
#include <iostream>
#include "ecs/Registry.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "scenes/JSONUtils.hpp"

/**
 * @struct GameMetadataComponent
 * @brief Component containing gameplay metadata for sandbox entities.
 */
struct GameMetadataComponent {
    /** @brief Importance value used to prioritize or classify entities. */
    float importance = 42.0f;
    /** @brief Tag string identifying the entity role or type. */
    std::string tag = "Hero";

    /**
     * @brief Serializes the GameMetadata component to JSON format.
     */
    static void serialize(Registry& reg, Entity entity, std::ostream& out, int indent) {
        if (auto* comp = reg.get<GameMetadataComponent>(entity)) {
            out << ",\n" << JSONUtils::indent(indent) << "\"importance\": " << comp->importance << ",\n";
            out << JSONUtils::indent(indent) << "\"tag\": " << JSONUtils::quote(comp->tag);
        }
    }

    /**
     * @brief Deserializes the GameMetadata component from JSON data.
     */
    static void deserialize(Registry& reg, VulkanRenderer&, Entity entity, const std::string& json) {
        float importance = 0.0f;
        if (JSONUtils::extractFloatValue(json, "importance", importance)) {
            std::string tag = JSONUtils::extractStringValue(json, "tag");
            reg.emplace<GameMetadataComponent>(entity, GameMetadataComponent{ importance, tag });
        }
    }
};
