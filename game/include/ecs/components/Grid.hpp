#pragma once
#include <glm/glm.hpp>

struct Grid : ComponentBase {


    uint32_t colorID;
    uint32_t meshID;

    float spacing = 1.0f;    // Distance between lines
    float size = 100.0f;     // Render area around the camera
    glm::vec4 color = { 0.5f, 0.5f, 0.5f, 1.0f };


    Grid(float s = 1.f, float sz = 100.f, glm::vec4 c = { 0.5f,0.5f,0.5f,1.f })
        : spacing(s), size(sz), color(c) {
    }
};
