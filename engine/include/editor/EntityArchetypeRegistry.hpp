#pragma once
#include <string>
#include <vector>
#include <functional>
#include "core/EngineAPI.hpp"

namespace Engine {

    /**
     * @struct EntityArchetype
     * @brief Describes a pre-defined entity template that appears in the "Create" and
     *        "Create Child" menus in the hierarchy panel.
     *
     * The `menuPath` drives the UI hierarchy:
     *   "3D Objects/Cube"           → BeginMenu("3D Objects") → MenuItem("Cube")
     *   "Rendering & Lights/Camera" → BeginMenu("Rendering & Lights") → MenuItem("Camera")
     *
     * Special paths:
     *   A path with no '/' is placed at the top level (not inside any submenu).
     *
     * The separator flag inserts an ImGui::Separator() **before** this item's menu entry.
     */
    struct EntityArchetype {
        /** @brief Full menu path, e.g. "3D Objects/Cube". */
        std::string menuPath;
        /** @brief Type string passed to Scene::createPrimitiveEntity or createEntityOfType. */
        std::string typeString;
        /** @brief If true, uses createPrimitiveEntity; otherwise createEntityOfType. */
        bool isPrimitive = false;
        /** @brief Insert a separator line before this entry in its submenu. */
        bool separatorBefore = false;
    };

    /**
     * @class EntityArchetypeRegistry
     * @brief Singleton that holds all registered entity archetypes in insertion order.
     *
     * Populate via REGISTER_ENTITY_ARCHETYPE (or REGISTER_ENTITY_ARCHETYPE_SEP for entries
     * that need a separator before them). The hierarchy panel reads this registry at runtime
     * to build its "Create" / "Create Child" menus automatically.
     */
    class ENGINE_API EntityArchetypeRegistry {
    public:
        static EntityArchetypeRegistry& getInstance();

        /**
         * @brief Register a new entity archetype.
         * @param archetype The archetype descriptor to add.
         */
        void registerArchetype(const EntityArchetype& archetype);

        /**
         * @return All registered archetypes in insertion order.
         */
        const std::vector<EntityArchetype>& getArchetypes() const;

    private:
        EntityArchetypeRegistry() = default;
        std::vector<EntityArchetype> archetypes;
    };

    ENGINE_API void registerBuiltinEntityArchetypes();

} // namespace Engine

// ---------------------------------------------------------------------------
// Registration Macros
// ---------------------------------------------------------------------------

/**
 * @brief Register an entity archetype that appears in hierarchy creation menus.
 *
 * @param MenuPath    Menu path, e.g. "3D Objects/Cube" or "UI/Canvas".
 * @param TypeString  String passed to Scene::createEntityOfType or createPrimitiveEntity.
 * @param IsPrimitive True for geometry primitives (Cube/Triangle/Quad), false otherwise.
 *
 * Example:
 *   REGISTER_ENTITY_ARCHETYPE("3D Objects/Cube",           "Cube",  true);
 *   REGISTER_ENTITY_ARCHETYPE("Rendering & Lights/Camera", "Camera", false);
 */
#define REGISTER_ENTITY_ARCHETYPE_IMPL(MenuPath, TypeString, IsPrimitive, SepBefore, Counter) \
    namespace { \
        struct AutoRegArch_##Counter { \
            AutoRegArch_##Counter() { \
                Engine::EntityArchetypeRegistry::getInstance().registerArchetype({ \
                    MenuPath, TypeString, IsPrimitive, SepBefore \
                }); \
            } \
        }; \
        static AutoRegArch_##Counter g_autoRegArch_##Counter; \
    }

#define REGISTER_ENTITY_ARCHETYPE_CONCAT(MenuPath, TypeString, IsPrimitive, SepBefore, Counter) \
    REGISTER_ENTITY_ARCHETYPE_IMPL(MenuPath, TypeString, IsPrimitive, SepBefore, Counter)

/** Register a normal archetype (no separator). */
#define REGISTER_ENTITY_ARCHETYPE(MenuPath, TypeString, IsPrimitive) \
    REGISTER_ENTITY_ARCHETYPE_CONCAT(MenuPath, TypeString, IsPrimitive, false, __COUNTER__)

/** Register an archetype with a separator line drawn before it in the menu. */
#define REGISTER_ENTITY_ARCHETYPE_SEP(MenuPath, TypeString, IsPrimitive) \
    REGISTER_ENTITY_ARCHETYPE_CONCAT(MenuPath, TypeString, IsPrimitive, true, __COUNTER__)
