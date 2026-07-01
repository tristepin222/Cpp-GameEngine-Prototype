#pragma once

/**
 * @enum PrimitiveKind
 * @brief Enum representing kinds of supported geometric primitives.
 */
enum class PrimitiveKind {
    Triangle,
    Cube,
    Quad
};

/**
 * @struct PrimitiveType
 * @brief Component containing the primitive kind configuration of an entity.
 */
struct PrimitiveType {
    /** @brief The kind of geometric primitive. */
    PrimitiveKind kind = PrimitiveKind::Triangle;
};
