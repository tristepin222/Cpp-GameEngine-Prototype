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
    // @reflect
    struct ENGINE_API TilemapComponent {
        // @reflect
        int width = 0;
        // @reflect
        int height = 0;
        // @reflect
        float tileSize = 1.0f;

        // @reflect
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
REGISTER_COMPONENT(Engine::TilemapComponent, "Rendering & Lights");

