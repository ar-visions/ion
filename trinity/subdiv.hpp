#pragma once
#include <math/math.hpp>


namespace ion {

    /// a 'mesh' is not renderable since we use elements in the verts
    /// so we define an import and export
    /// this is so we can import VBO/IBO, perform multiple iterations of sub div, then export to VBO/IBO
    struct Mesh:mx {
        static Mesh import_vbo(mx vbo, mx ibo, convert_tris);
        void export_vbo(mx &vbo, mx &ibo, bool export_tri);
        
        Mesh catmull_clark();
        mx_declare(Mesh, mx, IMesh)
    };

}

namespace std {
    template<>
    struct hash<ion::Edge> {
        size_t operator()(const ion::Edge &edge) const {
            return (size_t)(ion::u64(edge.a) << 32 | ion::u64(edge.b));
        }
    };
}
