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
                // 1. Merge fields by name
                if (!refl.fields.empty()) {
                    if (existing.fields.empty()) {
                        existing.fields = refl.fields;
                    } else {
                        for (const auto& newField : refl.fields) {
                            bool found = false;
                            for (auto& oldField : existing.fields) {
                                if (oldField.name == newField.name) {
                                    oldField = newField;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found) {
                                existing.fields.push_back(newField);
                            }
                        }
                    }
                }
                // 2. Merge category and display name
                if (!refl.category.empty() && refl.category != "General") existing.category = refl.category;
                if (!refl.displayName.empty()) existing.displayName = refl.displayName;
                // 3. Merge lifecycle callbacks
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


