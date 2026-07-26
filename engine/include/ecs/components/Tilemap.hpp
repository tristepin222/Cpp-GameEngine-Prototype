#pragma once
#include <string>
#include <vector>
#include "core/EngineAPI.hpp"

namespace Engine {

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


        /**
         * @brief 1D tile grid of size width*height.
         *        Value = index into TilesetAsset::tiles[].  -1 = empty cell.
         */
        std::vector<int> tiles;

        /** @brief Flag requesting mesh + collision rebuild next frame. */
        bool isDirty = true;
    };

} // namespace Engine

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Engine::TilemapComponent, "Rendering & Lights/Tilemap");

