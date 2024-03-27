#pragma once
#include <math/math.hpp>

namespace ion {
    struct IMesh;
    struct Mesh:mx {
        static Mesh import_vbo(mx vbo, mx ibo, bool convert_tris);
        void export_vbo(mx &vbo, mx &quads, mx &tris, bool export_tri);
        
        void catmull_clark();
        mx_declare(Mesh, mx, IMesh)
    };
}