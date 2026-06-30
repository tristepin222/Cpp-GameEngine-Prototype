#pragma once

enum class PrimitiveKind {
    Triangle,
    Cube,
    Quad
};

struct PrimitiveType {
    PrimitiveKind kind = PrimitiveKind::Triangle;
};
