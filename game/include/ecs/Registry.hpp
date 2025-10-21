// include/ecs/Registry.hpp
#pragma once
#include "EntityManager.hpp"
#include "ComponentStorage.hpp"
#include "Component.hpp"
#include <unordered_map>
#include <typeindex>
#include <functional>
#include <vector>

class Registry {
public:


    Registry() {
        entities.setRegistry(this);
    }


    using ComponentAddedCallback = std::function<void(Entity)>;
    using ComponentRemovedCallback = std::function<void(Entity)>;

    Entity create() { return entities.create(); }
    void destroy(Entity e) { entities.destroy(e); }

    template<typename T>
    T& emplace(Entity e, T&& comp) {
        ensureStorage<T>();
        getStorage<T>()->add(e, std::forward<T>(comp));

        auto id = registerComponentType(typeid(T));
        entities.getMask(e).set(id);

        // set back-pointers
        T* ptr = getStorage<T>()->get(e).value();
        ptr->entity = e;
        ptr->registry = this;

        if (componentAddedCallbacks.find(id) != componentAddedCallbacks.end())
            for (auto& cb : componentAddedCallbacks[id]) cb(e);

        // unwrap optional
        auto opt = getStorage<T>()->get(e);   // std::optional<T*>
        if (!opt.has_value()) throw std::runtime_error("Component missing after emplace!");
        return **opt; // dereference optional, then pointer, T&
    }



    template<typename T>
    void remove(Entity e) {
        auto* storage = getStorage<T>();
        if (storage && storage->has(e)) {
            storage->remove(e);
            auto id = registerComponentType(typeid(T));
            entities.getMask(e).reset(id);

            // notify systems
            if (componentRemovedCallbacks.find(id) != componentRemovedCallbacks.end()) {
                for (auto& cb : componentRemovedCallbacks[id]) cb(e);
            }
        }
    }

    template<typename T>
    T* get(Entity e) {
        ensureStorage<T>();
        auto opt = getStorage<T>()->get(e); // std::optional<T*>
        if (!opt.has_value()) return nullptr;
        return *opt; // unwrap optional → gives T*
    }

    template<typename T>
    T& getRef(Entity e) {
        T* ptr = get<T>(e);
        if (!ptr) throw std::runtime_error("Component missing!");
        return *ptr;
    }


    template<typename T>
    bool has(Entity e) {
        ensureStorage<T>();
        return getStorage<T>()->has(e);
    }

    // systems subscribe to component changes
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

    // Generates a view over all entities that have all Components
    template<typename... Components>
    auto view() {
        ensureStorages<Components...>();

        // Find the smallest storage to iterate efficiently
        auto* smallest = getSmallestStorage<Components...>();
        std::vector<Entity> result;
        if (!smallest) return View<Components...>(*this);

        // Iterate over smallest storage directly
        auto* typedSmallest = static_cast<ComponentStorage<std::tuple_element_t<0, std::tuple<Components...>>>*>(smallest);
        for (auto& [entity, _] : *typedSmallest) {
            if ((has<Components>(entity) && ...)) {
                result.push_back(entity);
            }
        }


        View<Components...> view(*this);
        view.entities = std::move(result);
        return view;
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
        return static_cast<ComponentStorage<T>*>(storages[id].get());
    }

    // Utility helpers
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
