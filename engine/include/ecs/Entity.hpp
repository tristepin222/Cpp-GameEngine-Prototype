#pragma once
#include <cstdint>
#include <bitset>
#include <limits>
#include <functional>

class Entity {
public:
    using IdType = std::uint32_t;
    static constexpr IdType INVALID_ENTITY = std::numeric_limits<IdType>::max();

    Entity() : id(INVALID_ENTITY) {}
    explicit Entity(IdType i) : id(i) {}

    IdType getId() const { return id; }

    bool operator==(const Entity& other) const { return id == other.id; }
    bool operator!=(const Entity& other) const { return id != other.id; }

private:
    IdType id;
};

constexpr std::size_t MAX_COMPONENTS = 64;
using ComponentMask = std::bitset<MAX_COMPONENTS>;
