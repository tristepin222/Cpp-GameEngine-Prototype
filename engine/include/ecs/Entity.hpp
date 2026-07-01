#pragma once
#include <cstdint>
#include <bitset>
#include <limits>
#include <functional>

/**
 * @class Entity
 * @brief Lightweight identifier representing a game object in the Entity Component System.
 */
class Entity {
public:
    /** @brief Numeric type of the entity identifier. */
    using IdType = std::uint32_t;
    /** @brief Constant representing a null or invalid entity. */
    static constexpr IdType INVALID_ENTITY = std::numeric_limits<IdType>::max();

    /**
     * @brief Construct a new, invalid Entity object.
     */
    Entity() : id(INVALID_ENTITY) {}
    /**
     * @brief Construct a new Entity object with a given ID.
     * @param i The identifier for the entity.
     */
    explicit Entity(IdType i) : id(i) {}

    /**
     * @brief Retrieve the raw identifier of the entity.
     * @return The raw ID.
     */
    IdType getId() const { return id; }

    /**
     * @brief Check equality with another entity.
     * @param other The entity to compare with.
     * @return True if IDs are identical, false otherwise.
     */
    bool operator==(const Entity& other) const { return id == other.id; }
    /**
     * @brief Check inequality with another entity.
     * @param other The entity to compare with.
     * @return True if IDs differ, false otherwise.
     */
    bool operator!=(const Entity& other) const { return id != other.id; }

private:
    /** @brief Unique numeric identifier. */
    IdType id;
};

/** @brief Maximum number of supported component types in the ECS. */
constexpr std::size_t MAX_COMPONENTS = 64;
/** @brief Bitmask representing component registration status. */
using ComponentMask = std::bitset<MAX_COMPONENTS>;
