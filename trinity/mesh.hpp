#pragma once
#include <trinity/trinity.hpp>

#ifndef MAX_VERTEX_SIZE
#define MAX_VERTEX_SIZE     64
#endif

namespace ion {

    struct Mesh:mx {
        struct M {
            Polygon     mode;
            int         level;
            mx          verts;
            array<u32>  quads;
            array<u32>  tris;
            array<u32>  wire;
        };

        /// returns the start to max level meshes using Pixar algorithm
        static array<Mesh> process(Mesh &mesh, const array<Polygon> &modes, int start_level, int max_level);
        static const Mesh &select(array<Mesh> &meshes, int level);
        mx_basic(Mesh)
    };
}