#include "meta/ComponentReflection.hpp"
#include <cstddef>

namespace Engine {

    ComponentReflectionRegistry& ComponentReflectionRegistry::getInstance() {
        static ComponentReflectionRegistry instance;
        return instance;
    }



    void ComponentReflectionRegistry::registerComponent(const ComponentReflection& refl) {
        for (const auto& existing : reflections) {
            if (existing.name == refl.name) return;
        }
        reflections.push_back(refl);
    }

    const std::vector<ComponentReflection>& ComponentReflectionRegistry::getReflections() const {
        return reflections;
    }

} // namespace Engine


