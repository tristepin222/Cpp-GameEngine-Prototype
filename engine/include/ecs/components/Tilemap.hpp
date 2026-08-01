#pragma once
#include <string>
#include <vector>
#include "core/EngineAPI.hpp"

namespace Engine {

    struct TilemapLayer {
        std::string name;
        std::vector<int> tiles;
        float zOffset = 0.0f;
        std::string tag;
        bool isVisible = true;
    };

    /**
     * @struct TilemapComponent
     * @brief Represents a 2D tile grid in the scene.
     *        References a tileset by its disk path (.tileset file).
     *        The TilemapSystem loads the tileset, builds a packed atlas, and
     *        generates a single mesh + material per tilemap entity.
     */
    // [ReflectClass]
    struct ENGINE_API TilemapComponent {
        // [ReflectField]
        int width = 0;
        // [ReflectField]
        int height = 0;
        // [ReflectField]
        float tileSize = 1.0f;

        // [ReflectField]
        std::string tilesetPath;


        // Dynamic layers (handled manually in serialization and inspector UI)
        std::vector<TilemapLayer> layers;

        // Deprecated compatibility fields
        std::vector<int> tiles;
        std::vector<int> obstacleTiles;

        /** @brief Flag requesting mesh + collision rebuild next frame. */
        bool isDirty = true;

        TilemapComponent() {
            TilemapLayer defaultLayer;
            defaultLayer.name = "Ground";
            defaultLayer.zOffset = 0.0f;
            defaultLayer.tag = "ground";
            defaultLayer.isVisible = true;
            layers.push_back(defaultLayer);
        }
    };

} // namespace Engine

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Engine::TilemapComponent, "Rendering & Lights/Tilemap");

