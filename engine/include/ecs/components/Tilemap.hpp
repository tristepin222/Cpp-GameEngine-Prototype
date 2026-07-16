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
    struct ENGINE_API TilemapComponent {
        /** @brief Grid width in tiles. */
        int width = 0;
        /** @brief Grid height in tiles. */
        int height = 0;
        /** @brief World-space size of one tile cell. */
        float tileSize = 1.0f;

        /**
         * @brief Path to the .tileset asset file (e.g. "assets/tilesets/forest.tileset").
         *        Stored as a project-relative path.
         */
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
