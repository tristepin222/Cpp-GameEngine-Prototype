#include "scenes/TilesetAsset.hpp"
#include "renderer/VulkanRenderer.hpp"
#include "renderer/ResourceManager.hpp"
#include "scenes/JSONUtils.hpp"

#include "stb_image.h"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <cmath>
#include <cstring>

namespace Engine {

// ---------------------------------------------------------------------------
// Process-wide cache
// ---------------------------------------------------------------------------
std::unordered_map<std::string, TilesetAsset>& getTilesetCache() {
    static std::unordered_map<std::string, TilesetAsset> s_cache;
    return s_cache;
}

void invalidateTilesetCache(const std::string& path) {
    auto& cache = getTilesetCache();
    if (path.empty()) {
        cache.clear();
    } else {
        cache.erase(path);
    }
}

TilesetAsset* loadOrGetTileset(const std::string& path, VulkanRenderer& renderer) {
    if (path.empty()) return nullptr;
    auto& cache = getTilesetCache();

    auto it = cache.find(path);
    if (it != cache.end()) {
        // Rebuild atlas if it hasn't been built yet
        if (!it->second.atlas.valid) {
            it->second.buildAtlas(renderer);
        }
        return &it->second;
    }

    // Load from disk
    TilesetAsset ts = TilesetAsset::loadFromFile(path);
    if (ts.filePath.empty()) {
        std::cerr << "[TilesetAsset] Failed to load tileset from: " << path << std::endl;
        return nullptr;
    }

    ts.buildAtlas(renderer);
    cache[path] = std::move(ts);
    return &cache[path];
}

// ---------------------------------------------------------------------------
// Disk I/O — .tile files
// ---------------------------------------------------------------------------

TileAsset TilesetAsset::loadTileFile(const std::string& path) {
    TileAsset tile;
    if (!std::filesystem::exists(path)) return tile;

    std::ifstream f(path);
    if (!f.is_open()) return tile;

    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    float idVal = -1.f;
    JSONUtils::extractFloatValue(json, "id", idVal);
    tile.id          = static_cast<int>(idVal);
    tile.name        = JSONUtils::extractStringValue(json, "name");
    tile.texturePath = JSONUtils::extractStringValue(json, "texturePath");

    float solidVal = 0.f;
    JSONUtils::extractFloatValue(json, "isSolid", solidVal);
    tile.isSolid = solidVal != 0.f;

    float gx = 0.f, gy = 0.f;
    JSONUtils::extractFloatValue(json, "gridX", gx);
    JSONUtils::extractFloatValue(json, "gridY", gy);
    tile.gridX = static_cast<int>(gx);
    tile.gridY = static_cast<int>(gy);

    return tile;
}

void TilesetAsset::saveTileFile(const TileAsset& tile, const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream f(path);
    if (!f.is_open()) {
        std::cerr << "[TilesetAsset] Cannot write .tile file: " << path << std::endl;
        return;
    }
    f << "{\n";
    f << "  \"id\": " << tile.id << ",\n";
    f << "  \"name\": \"" << tile.name << "\",\n";
    f << "  \"texturePath\": \"" << tile.texturePath << "\",\n";
    f << "  \"isSolid\": " << (tile.isSolid ? 1 : 0) << ",\n";
    f << "  \"gridX\": " << tile.gridX << ",\n";
    f << "  \"gridY\": " << tile.gridY << "\n";
    f << "}\n";
}

// ---------------------------------------------------------------------------
// Disk I/O — .tileset files
// ---------------------------------------------------------------------------

TilesetAsset TilesetAsset::loadFromFile(const std::string& path) {
    TilesetAsset ts;
    if (!std::filesystem::exists(path)) return ts;

    std::ifstream f(path);
    if (!f.is_open()) return ts;

    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    ts.name     = JSONUtils::extractStringValue(json, "name");
    ts.filePath = path;

    float tw = 16.f, th = 16.f;
    JSONUtils::extractFloatValue(json, "tileWidth",  tw);
    JSONUtils::extractFloatValue(json, "tileHeight", th);
    ts.tileWidth  = static_cast<int>(tw);
    ts.tileHeight = static_cast<int>(th);

    // Parse the "tiles" array — array of .tile file paths
    std::vector<std::string> tilePaths;
    JSONUtils::extractStringVector(json, "tiles", tilePaths);

    // Resolve relative paths to be relative to the .tileset file's parent directory
    std::filesystem::path tsDir = std::filesystem::path(path).parent_path();
    int idx = 0;
    for (const auto& tp : tilePaths) {
        std::string resolvedPath = tp;
        if (!std::filesystem::exists(resolvedPath)) {
            // Try relative to tileset dir
            resolvedPath = (tsDir / tp).generic_string();
        }
        TileAsset tile = loadTileFile(resolvedPath);
        tile.id = idx++;
        ts.tiles.push_back(std::move(tile));
    }

    return ts;
}

void TilesetAsset::saveToFile(const TilesetAsset& ts) {
    if (ts.filePath.empty()) {
        std::cerr << "[TilesetAsset] Cannot save tileset: filePath is empty." << std::endl;
        return;
    }

    std::filesystem::create_directories(std::filesystem::path(ts.filePath).parent_path());
    std::ofstream f(ts.filePath);
    if (!f.is_open()) {
        std::cerr << "[TilesetAsset] Cannot write .tileset file: " << ts.filePath << std::endl;
        return;
    }

    f << "{\n";
    f << "  \"name\": \"" << ts.name << "\",\n";
    f << "  \"tileWidth\": " << ts.tileWidth << ",\n";
    f << "  \"tileHeight\": " << ts.tileHeight << ",\n";
    f << "  \"tiles\": [\n";

    std::filesystem::path tsDir = std::filesystem::path(ts.filePath).parent_path();
    for (size_t i = 0; i < ts.tiles.size(); ++i) {
        // Store paths relative to the .tileset file's directory
        std::string tilePath = ts.tiles[i].name + ".tile";
        std::filesystem::path absTilePath = tsDir / tilePath;
        std::string storedPath = absTilePath.generic_string();
        f << "    \"" << storedPath << "\"";
        if (i + 1 < ts.tiles.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n";
    f << "}\n";
}

// ---------------------------------------------------------------------------
// Atlas building
// ---------------------------------------------------------------------------

void TilesetAsset::buildAtlas(VulkanRenderer& renderer) {
    atlas = AtlasCache{};

    if (tiles.empty()) {
        atlas.valid = true; // empty but valid
        return;
    }

    const int cellW = (tileWidth  > 0) ? tileWidth  : 16;
    const int cellH = (tileHeight > 0) ? tileHeight : 16;

    // Compute atlas grid dimensions (square-ish layout)
    int numTiles   = static_cast<int>(tiles.size());
    int gridCols   = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(numTiles))));
    int gridRows   = (numTiles + gridCols - 1) / gridCols;

    int atlasW = gridCols * cellW;
    int atlasH = gridRows * cellH;

    std::vector<uint8_t> atlasPx(atlasW * atlasH * 4, 0); // RGBA, default transparent black

    // Solid magenta placeholder for missing textures (4 bytes: R G B A)
    constexpr uint8_t MAGENTA[4] = { 255, 0, 255, 255 };

    for (int ti = 0; ti < numTiles; ++ti) {
        TileAsset& tile = tiles[ti];

        int col = ti % gridCols;
        int row = ti / gridCols;

        // Compute UV rect for this tile
        float u0 = static_cast<float>(col * cellW) / static_cast<float>(atlasW);
        float v0 = static_cast<float>(row * cellH) / static_cast<float>(atlasH);
        float u1 = static_cast<float>((col + 1) * cellW) / static_cast<float>(atlasW);
        float v1 = static_cast<float>((row + 1) * cellH) / static_cast<float>(atlasH);
        tile.atlasUV = glm::vec4(u0, v0, u1, v1);

        // Load tile texture pixels
        int tw = 0, th = 0, tc = 0;
        stbi_uc* srcPx = nullptr;
        if (!tile.texturePath.empty() && std::filesystem::exists(tile.texturePath)) {
            stbi_set_flip_vertically_on_load(false);
            srcPx = stbi_load(tile.texturePath.c_str(), &tw, &th, &tc, STBI_rgb_alpha);
        }

        // Blit tile pixels into the atlas (with scaling if needed)
        int dstX = col * cellW;
        int dstY = row * cellH;

        for (int py = 0; py < cellH; ++py) {
            for (int px = 0; px < cellW; ++px) {
                int dstIdx = ((dstY + py) * atlasW + (dstX + px)) * 4;

                if (srcPx && tw > 0 && th > 0) {
                    // Nearest-neighbour scale from source → cellW x cellH
                    int srcPxX = (px * tw) / cellW;
                    int srcPxY = (py * th) / cellH;
                    int srcIdx = (srcPxY * tw + srcPxX) * 4;
                    atlasPx[dstIdx + 0] = srcPx[srcIdx + 0];
                    atlasPx[dstIdx + 1] = srcPx[srcIdx + 1];
                    atlasPx[dstIdx + 2] = srcPx[srcIdx + 2];
                    atlasPx[dstIdx + 3] = srcPx[srcIdx + 3];
                } else {
                    // Magenta placeholder
                    atlasPx[dstIdx + 0] = MAGENTA[0];
                    atlasPx[dstIdx + 1] = MAGENTA[1];
                    atlasPx[dstIdx + 2] = MAGENTA[2];
                    atlasPx[dstIdx + 3] = MAGENTA[3];
                }
            }
        }

        if (srcPx) stbi_image_free(srcPx);
    }

    // Upload atlas to GPU via ResourceManager
    std::string cacheKey = "tileset_atlas:" + filePath;
    Texture* tex = renderer.resourceManager->createTextureFromPixels(
        cacheKey, atlasPx.data(), atlasW, atlasH, renderer, TextureFilterMode::Nearest
    );

    if (tex) {
        atlas.pixels      = std::move(atlasPx);
        atlas.atlasWidth  = atlasW;
        atlas.atlasHeight = atlasH;
        atlas.descriptorSet = tex->descriptorSet;
        atlas.valid       = true;
    } else {
        std::cerr << "[TilesetAsset] Atlas GPU upload failed for: " << filePath << std::endl;
    }
}

void TilesetAsset::destroyAtlas(VulkanRenderer& renderer) {
    if (!atlas.valid) return;
    std::string cacheKey = "tileset_atlas:" + filePath;
    renderer.resourceManager->evictTexture(cacheKey, renderer.device.getDevice());
    atlas = AtlasCache{};
}

} // namespace Engine
