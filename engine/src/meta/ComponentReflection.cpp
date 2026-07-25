#include "meta/ComponentReflection.hpp"
#include <cstddef>

namespace Engine {

    ComponentReflectionRegistry& ComponentReflectionRegistry::getInstance() {
        static ComponentReflectionRegistry instance;
        return instance;
    }



    void ComponentReflectionRegistry::registerComponent(const ComponentReflection& refl) {
        for (auto& existing : reflections) {
            if (existing.name == refl.name) {
                if (existing.fields.empty() && !refl.fields.empty()) {
                    existing.fields = refl.fields;
                }
                if (refl.add) existing.add = refl.add;
                if (refl.has) existing.has = refl.has;
                if (refl.remove) existing.remove = refl.remove;
                if (refl.get) existing.get = refl.get;
                return;
            }
        }
        reflections.push_back(refl);
    }



    const std::vector<ComponentReflection>& ComponentReflectionRegistry::getReflections() const {
        return reflections;
    }

} // namespace Engine


