
/// cgi module, in ion namespace
/// simplified 3D interface to handle the majority of use-case

/// interface:
///
/// Vertex class <mx struct [attrs]>
/// Shader class <mx struct [uniform]>
/// 
/// 

#include <math/math.hpp>

///
namespace ion {

struct vdata:mx {
    ptr_declare(vdata, mx, struct _vertex);
    vdata(mx vbo, array<u32> ibo);
};

template <typename A>
struct vertex:vdata {
    vertex(array<A> vbo, array<u32> ibo):vdata(typeof(A)) { }
};

}