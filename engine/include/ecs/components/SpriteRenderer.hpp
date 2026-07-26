#pragma once
#include <glm/glm.hpp>
#include <string>
#include "core/EngineAPI.hpp"

namespace Engine {

    /**
     * @struct SpriteRenderer
     * @brief Component that renders a textured 2D quad (sprite) on an entity.
     *
     * The SpriteSystem manages a unit quad Mesh and an "Unlit" Material per sprite entity
     * so the existing RenderSystem picks them up automatically. Use with an orthographic Camera
     * for traditional 2D game rendering, or combine with a perspective Camera for 3D billboards.
     *
     * Flip is handled via UV remapping in the sprite.vert shader — flipping does not affect
     * the entity's Transform, Collider, or any physics component.
     *
     * Sort order is stored here for depth hint purposes. The SpriteSystem automatically adjusts
     * Transform.position.z by sortOrder * 0.0001f each frame so overlapping sprites layer correctly.
     * Users should keep Transform.z at 0 and use sortOrder for layering.
     */
    // [ReflectClass]
    struct ENGINE_API SpriteRenderer {
        /** @brief Path to the sprite texture asset (PNG, JPG, TGA). */
        // [ReflectField]
        std::string texturePath;

        /** @brief RGBA tint multiplied with the sampled texture colour. */
        // [ReflectField]
        glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };

        /** @brief Flip UV horizontally (mirrors sprite around the vertical axis). */
        // [ReflectField]
        bool flipX = false;

        /** @brief Flip UV vertically (mirrors sprite around the horizontal axis). */
        // [ReflectField]
        bool flipY = false;

        /** @brief Depth-sorting order relative to other sprites. Lower values render behind higher values. */
        // [ReflectField]
        int sortOrder = 0;

        // ---- Internal runtime state (not reflected, not serialized) ----

        /** @brief GPU mesh ID of the managed unit quad (0 = not yet created). */
        uint32_t _managedMeshId  = 0;

        /** @brief GPU material ID of the managed sprite material (0 = not yet created). */
        uint32_t _managedMatId   = 0;

        /** @brief The texture path that was last uploaded — used to detect dirty texture changes. */
        std::string _loadedTexturePath;

        /** @brief The flip state that was last applied — used to detect changes requiring mesh rebuild. */
        bool _lastFlipX = false;

        /** @brief The flip state that was last applied — used to detect changes requiring mesh rebuild. */
        bool _lastFlipY = false;

        /** @brief sortOrder value that was last applied. */
        int _lastSortOrder = 0;

        /** @brief Whether this sprite needs full setup on the next update. */
        bool _dirty = true;
    };

} // namespace Engine

#include "meta/ComponentReflection.hpp"
REGISTER_COMPONENT(Engine::SpriteRenderer, "Rendering & Lights/Sprite Renderer");
