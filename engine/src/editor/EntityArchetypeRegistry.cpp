#include "editor/EntityArchetypeRegistry.hpp"

namespace Engine {

    EntityArchetypeRegistry& EntityArchetypeRegistry::getInstance() {
        static EntityArchetypeRegistry instance;
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
            registerBuiltinEntityArchetypes();
        }
        return instance;
    }

    void EntityArchetypeRegistry::registerArchetype(const EntityArchetype& archetype) {
        archetypes.push_back(archetype);
    }

    const std::vector<EntityArchetype>& EntityArchetypeRegistry::getArchetypes() const {
        return archetypes;
    }

} // namespace Engine
