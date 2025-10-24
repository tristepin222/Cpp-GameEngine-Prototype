// EntityHash.hpp
#pragma once
#include "Entity.hpp"
#include <functional>

namespace std {
    template<>
    struct hash<Entity> {
        inline std::size_t operator()(const Entity& e) const noexcept {
            return std::hash<Entity::IdType>{}(e.getId());
        }
    };
}