#pragma once
#include "Mesh.hpp"

/**
 * @namespace Primitives
 * @brief Collection of helper functions for generating standard geometric primitive meshes.
 */
namespace Primitives {

    /**
     * @brief Creates a 2D triangle mesh.
     * @return The generated Mesh object.
     */
    inline Mesh makeTriangle() {
        return Mesh{
            {
                Vertex({-0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,0.f}),
                Vertex({ 0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,0.f}),
                Vertex({ 0.0f,  0.5f, 0.f}, {0.f,0.f,1.f}, {0.5f,1.f})
            },
            {0, 1, 2}
        };
    }

    /**
     * @brief Creates a 2D quad (square) mesh.
     * @return The generated Mesh object.
     */
    inline Mesh makeQuad() {
        return Mesh{
            {
                Vertex({-0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,0.f}),
                Vertex({ 0.5f, -0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,0.f}),
                Vertex({ 0.5f,  0.5f, 0.f}, {0.f,0.f,1.f}, {1.f,1.f}),
                Vertex({-0.5f,  0.5f, 0.f}, {0.f,0.f,1.f}, {0.f,1.f}),
            },
            {0, 1, 2, 2, 3, 0}
        };
    }

    /**
     * @brief Creates a 3D cube mesh.
     * @return The generated Mesh object.
     */
    inline Mesh makeCube() {
        std::vector<Vertex> verts = {
            // front
            {{-0.5f,-0.5f, 0.5f},{0,0,1},{0,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,0,1},{1,0}},
            {{ 0.5f, 0.5f, 0.5f},{0,0,1},{1,1}},
            {{-0.5f, 0.5f, 0.5f},{0,0,1},{0,1}},
            // back
            {{-0.5f,-0.5f,-0.5f},{0,0,-1},{1,0}},
            {{ 0.5f,-0.5f,-0.5f},{0,0,-1},{0,0}},
            {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{0,1}},
            {{-0.5f, 0.5f,-0.5f},{0,0,-1},{1,1}},
        };
        std::vector<uint32_t> inds = {
            0,1,2, 2,3,0,  // front
            1,5,6, 6,2,1,  // right
            5,4,7, 7,6,5,  // back
            4,0,3, 3,7,4,  // left
            3,2,6, 6,7,3,  // top
            4,5,1, 1,0,4   // bottom
        };
        return Mesh{ verts, inds };
    }

}
