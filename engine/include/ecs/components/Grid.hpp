#pragma once
#include <glm/glm.hpp>

/**
 * @struct Grid
 * @brief Represents a grid component used for rendering a grid overlay.
 */
struct Grid {


    /** @brief ID associated with the grid color. */
    uint32_t colorID;
    /** @brief ID associated with the grid mesh. */
    uint32_t meshID;

    /** @brief Spacing between grid lines. */
    float spacing = 1.0f;    // Distance between lines
    /** @brief Render size area around the camera. */
    float size = 100.0f;     // Render area around the camera
    /** @brief Color of the grid lines. */
    glm::vec4 color = { 0.5f, 0.5f, 0.5f, 1.0f };


    /**
     * @brief Construct a new Grid object.
     * @param s Distance between lines.
     * @param sz Render area around the camera.
     * @param c Color of the grid lines.
     */
    Grid(float s = 1.f, float sz = 100.f, glm::vec4 c = { 0.5f,0.5f,0.5f,1.f })
        : spacing(s), size(sz), color(c) {
    }
};
