#pragma once
#include "EntityManager.hpp"
#include "ComponentStorage.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <vector>
#include <memory>

// Simple runtime component type id registry
inline std::unordered_map<std::type_index, std::size_t>& g_componentTypeMap() {
    static std::unordered_map<std::type_index, std::size_t> map;
    return map;
}

inline std::size_t registerComponentType(std::type_index idx) {
    auto &map = g_componentTypeMap();
    if (map.find(idx) == map.end()) {
        std::size_t id = map.size();
        map[idx] = id;
        return id;
    }
    return map[idx];
}

class Registry {
public:
    Registry() = default;

    using ComponentAddedCallback = std::function<void(Entity)>;
    using ComponentRemovedCallback = std::function<void(Entity)>;

    Entity create() { return entities.create(); }
    void destroy(Entity e) {
        if (e.getId() == Entity::INVALID_ENTITY) {
            return;
        }

        ComponentMask mask = entities.getMask(e);
        for (auto& [id, storage] : storages) {
            if (!mask.test(id)) {
                continue;
            }

            storage->removeEntity(e);

            auto callbacksIt = componentRemovedCallbacks.find(id);
            if (callbacksIt != componentRemovedCallbacks.end()) {
                for (auto& cb : callbacksIt->second) {
                    cb(e);
                }
            }
        }

        entities.destroy(e);
    }

    // Add or emplace component
    template<typename T>
    T& emplace(Entity e, T&& comp) {
        ensureStorage<T>();
        getStorage<T>()->add(e, std::forward<T>(comp));

        auto id = registerComponentType(typeid(T));
        entities.getMask(e).set(id);

        if (componentAddedCallbacks.find(id) != componentAddedCallbacks.end()) {
            for (auto& cb : componentAddedCallbacks[id]) cb(e);
        }

        return getStorage<T>()->get(e);
    }

    // Remove component
    template<typename T>
    void remove(Entity e) {
        auto* storage = getStorage<T>();
        if (storage && storage->has(e)) {
            storage->remove(e);
            auto id = registerComponentType(typeid(T));
            entities.getMask(e).reset(id);

            if (componentRemovedCallbacks.find(id) != componentRemovedCallbacks.end()) {
                for (auto& cb : componentRemovedCallbacks[id]) cb(e);
            }
        }
    }

    // Access component
    template<typename T>
    T* get(Entity e) {
        auto* storage = getStorage<T>();
        if (!storage || !storage->has(e)) return nullptr;
        return &storage->get(e);
    }

    template<typename T>
    T& getRef(Entity e) {
        T* ptr = get<T>(e);
        if (!ptr) throw std::runtime_error("Component missing!");
        return *ptr;
    }

    template<typename T>
    bool has(Entity e) {
        auto* storage = getStorage<T>();
        return storage && storage->has(e);
    }

    // Subscribe to component events
    template<typename T>
    void subscribeToAdded(ComponentAddedCallback cb) {
        auto id = registerComponentType(typeid(T));
        componentAddedCallbacks[id].push_back(cb);
    }

    template<typename T>
    void subscribeToRemoved(ComponentRemovedCallback cb) {
        auto id = registerComponentType(typeid(T));
        componentRemovedCallbacks[id].push_back(cb);
    }

    // --- Views ---
    template<typename... Components>
    class View {
    public:
        View(Registry& reg) : registry(reg) {}

        struct Iterator {
            Registry& registry;
            std::vector<Entity>::iterator it;

            bool operator!=(const Iterator& other) const { return it != other.it; }
            void operator++() { ++it; }

            auto operator*() const {
                Entity e = *it;
                return std::tuple_cat(std::make_tuple(e), std::forward_as_tuple(*registry.get<Components>(e)...));
            }
        };

        Iterator begin() { return Iterator{ registry, entities.begin() }; }
        Iterator end() { return Iterator{ registry, entities.end() }; }

        std::vector<Entity> entities;
        Registry& registry;
    };

    template<typename... Components>
    auto view() {
        ensureStorages<Components...>();

        // pick the smallest storage to iterate
        IStorage* smallest = getSmallestStorage<Components...>();
        std::vector<Entity> result;

        if (smallest) {
            for (Entity e : smallest->getEntities()) {
                if ((has<Components>(e) && ...)) result.push_back(e);
            }
        }

        View<Components...> v(*this);
        v.entities = std::move(result);
        return v;
    }

private:
    EntityManager entities;
    std::unordered_map<std::size_t, std::unique_ptr<IStorage>> storages;
    std::unordered_map<std::size_t, std::vector<ComponentAddedCallback>> componentAddedCallbacks;
    std::unordered_map<std::size_t, std::vector<ComponentRemovedCallback>> componentRemovedCallbacks;

    template<typename T>
    void ensureStorage() {
        std::size_t id = registerComponentType(typeid(T));
        if (storages.find(id) == storages.end()) {
            storages[id] = std::make_unique<ComponentStorage<T>>();
        }
    }

    template<typename T>
    ComponentStorage<T>* getStorage() {
        std::size_t id = registerComponentType(typeid(T));
        auto it = storages.find(id);
        if (it == storages.end()) return nullptr;
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }

    template<typename... Components>
    void ensureStorages() { (ensureStorage<Components>(), ...); }

    template<typename T>
    IStorage* getSmallestStorage() { return getStorage<T>(); }

    template<typename First, typename Second, typename... Rest>
    IStorage* getSmallestStorage() {
        IStorage* a = getStorage<First>();
        IStorage* b = getSmallestStorage<Second, Rest...>();
        if (!a) return b;
        if (!b) return a;
        return (a->size() < b->size()) ? a : b;
    }
};
