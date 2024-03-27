#pragma once
#include <math/math.hpp>

namespace ion {
    struct IMesh;
    struct Mesh:mx {
        static Mesh import_vbo(mx vbo, array<u32> ibo, bool convert_tris);
        void export_vbo(mx &vbo, array<u32> &quads, array<u32> &tris, bool export_tri);
        
        void catmull_clark();
        mx_declare(Mesh, mx, IMesh)
    };
}