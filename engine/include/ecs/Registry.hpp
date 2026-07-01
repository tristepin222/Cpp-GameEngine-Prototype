#pragma once
#include "EntityManager.hpp"
#include "ComponentStorage.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <vector>
#include <memory>

// Simple runtime component type id registry
/**
 * @brief Retrieves the global map of component types to unique numeric IDs.
 * @return Reference to the type index map.
 */
inline std::unordered_map<std::type_index, std::size_t>& g_componentTypeMap() {
    static std::unordered_map<std::type_index, std::size_t> map;
    return map;
}

/**
 * @brief Registers or retrieves the unique ID for a component type.
 * @param idx The type index.
 * @return The unique ID of the component type.
 */
inline std::size_t registerComponentType(std::type_index idx) {
    auto &map = g_componentTypeMap();
    if (map.find(idx) == map.end()) {
        std::size_t id = map.size();
        map[idx] = id;
        return id;
    }
    return map[idx];
}

/**
 * @class Registry
 * @brief Coordinates entities and components, providing creation, removal, lookup, and views.
 */
class Registry {
public:
    /**
     * @brief Construct a new Registry object.
     */
    Registry() = default;

    /** @brief Callback function type invoked when a component is added. */
    using ComponentAddedCallback = std::function<void(Entity)>;
    /** @brief Callback function type invoked when a component is removed. */
    using ComponentRemovedCallback = std::function<void(Entity)>;

    /**
     * @brief Creates a new entity.
     * @return The created entity.
     */
    Entity create() { return entities.create(); }
    
    /**
     * @brief Destroys an entity, removing all its components.
     * @param e Entity to destroy.
     */
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

    /**
     * @brief Emplaces a component on an entity.
     * @tparam T Component type to emplace.
     * @param e Entity to emplace component on.
     * @param comp Component instance to add.
     * @return Reference to the emplaced component.
     */
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

    /**
     * @brief Removes a component of type T from an entity.
     * @tparam T Component type to remove.
     * @param e Target entity.
     */
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

    /**
     * @brief Retrieves a pointer to a component of type T on an entity.
     * @tparam T Component type.
     * @param e Target entity.
     * @return Pointer to the component, or nullptr if not found.
     */
    template<typename T>
    T* get(Entity e) {
        auto* storage = getStorage<T>();
        if (!storage || !storage->has(e)) return nullptr;
        return &storage->get(e);
    }

    /**
     * @brief Retrieves a reference to a component of type T on an entity. Throws if missing.
     * @tparam T Component type.
     * @param e Target entity.
     * @return Reference to the component.
     */
    template<typename T>
    T& getRef(Entity e) {
        T* ptr = get<T>(e);
        if (!ptr) throw std::runtime_error("Component missing!");
        return *ptr;
    }

    /**
     * @brief Checks if an entity possesses a component of type T.
     * @tparam T Component type.
     * @param e Target entity.
     * @return True if component exists, false otherwise.
     */
    template<typename T>
    bool has(Entity e) {
        auto* storage = getStorage<T>();
        return storage && storage->has(e);
    }

    /**
     * @brief Subscribes a callback to when a component of type T is added.
     * @tparam T Component type.
     * @param cb Callback function.
     */
    template<typename T>
    void subscribeToAdded(ComponentAddedCallback cb) {
        auto id = registerComponentType(typeid(T));
        componentAddedCallbacks[id].push_back(cb);
    }

    /**
     * @brief Subscribes a callback to when a component of type T is removed.
     * @tparam T Component type.
     * @param cb Callback function.
     */
    template<typename T>
    void subscribeToRemoved(ComponentRemovedCallback cb) {
        auto id = registerComponentType(typeid(T));
        componentRemovedCallbacks[id].push_back(cb);
    }

    /**
     * @class View
     * @brief Filtered view over entities containing a specific set of components.
     * @tparam Components Component filter list.
     */
    template<typename... Components>
    class View {
    public:
        /**
         * @brief Construct a new View object.
         * @param reg Registry creating this view.
         */
        View(Registry& reg) : registry(reg) {}

        /**
         * @struct Iterator
         * @brief Iterator over the entities matching the View filters.
         */
        struct Iterator {
            /** @brief Reference to the Registry. */
            Registry& registry;
            /** @brief Iterator inside result entity vector. */
            std::vector<Entity>::iterator it;

            /** @brief Check iterator inequality. */
            bool operator!=(const Iterator& other) const { return it != other.it; }
            /** @brief Move iterator forward. */
            void operator++() { ++it; }

            /**
             * @brief Dereferences the iterator to a tuple containing the entity and its component references.
             * @return A tuple of (Entity, T&...).
             */
            auto operator*() const {
                Entity e = *it;
                return std::tuple_cat(std::make_tuple(e), std::forward_as_tuple(*registry.get<Components>(e)...));
            }
        };

        /** @brief Returns beginning view iterator. */
        Iterator begin() { return Iterator{ registry, entities.begin() }; }
        /** @brief Returns ending view iterator. */
        Iterator end() { return Iterator{ registry, entities.end() }; }

        /** @brief Entities that match component criteria. */
        std::vector<Entity> entities;
        /** @brief Reference to registry. */
        Registry& registry;
    };

    /**
     * @brief Generates a filtered view of entities containing the requested components.
     * @tparam Components Components to filter by.
     * @return A View object.
     */
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
    /** @brief Entity lifetime manager. */
    EntityManager entities;
    /** @brief Map of component storage classes keyed by type ID. */
    std::unordered_map<std::size_t, std::unique_ptr<IStorage>> storages;
    /** @brief Callbacks for component insertions. */
    std::unordered_map<std::size_t, std::vector<ComponentAddedCallback>> componentAddedCallbacks;
    /** @brief Callbacks for component removals. */
    std::unordered_map<std::size_t, std::vector<ComponentRemovedCallback>> componentRemovedCallbacks;

    /**
     * @brief Ensures a component storage exists.
     * @tparam T Component type.
     */
    template<typename T>
    void ensureStorage() {
        std::size_t id = registerComponentType(typeid(T));
        if (storages.find(id) == storages.end()) {
            storages[id] = std::make_unique<ComponentStorage<T>>();
        }
    }

    /**
     * @brief Retrieves storage class for component type.
     * @tparam T Component type.
     * @return Typed storage pointer, or nullptr if none.
     */
    template<typename T>
    ComponentStorage<T>* getStorage() {
        std::size_t id = registerComponentType(typeid(T));
        auto it = storages.find(id);
        if (it == storages.end()) return nullptr;
        return static_cast<ComponentStorage<T>*>(it->second.get());
    }

    /**
     * @brief Ensures all given component type storages exist.
     * @tparam Components Pack of component types.
     */
    template<typename... Components>
    void ensureStorages() { (ensureStorage<Components>(), ...); }

    /**
     * @brief Base case for retrieving smallest storage from parameter pack.
     * @tparam T Component type.
     * @return Base storage interface.
     */
    template<typename T>
    IStorage* getSmallestStorage() { return getStorage<T>(); }

    /**
     * @brief Recursively determines smallest storage among requested components.
     * @tparam First First type.
     * @tparam Second Second type.
     * @tparam Rest Remaining types.
     * @return Base storage interface pointer of smallest storage.
     */
    template<typename First, typename Second, typename... Rest>
    IStorage* getSmallestStorage() {
        IStorage* a = getStorage<First>();
        IStorage* b = getSmallestStorage<Second, Rest...>();
        if (!a) return b;
        if (!b) return a;
        return (a->size() < b->size()) ? a : b;
    }
};
