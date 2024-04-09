#include <trinity/trinity.hpp>
#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>

namespace ion {

/// project:
/// we want to perform this calculation using Dawn instance, with WebGPU compute shader used in osd
/// submit pull-request diff with that patch!
/// for that, we would convert GLSL to WGL

Array<Mesh> Mesh::process(Mesh &mesh, const Array<Polygon> &modes, int start_level, int max_level) {

    type_t mtype = mesh->verts.type();
    type_t vtype = (mtype->traits & traits::array) ? mtype->schema->bind->data : mtype;
    int    vertex_size = vtype->base_sz;
    float *f_origin = mesh->verts.origin<float>();
    
    using namespace OpenSubdiv;
    /// assert we are not overflowing the MAX_VERTEX_SIZE defined

    assert(vertex_size % sizeof(float) == 0);
    assert(vertex_size / sizeof(float) <= MAX_VERTEX_SIZE);
    static int n_floats;
    n_floats = vertex_size / sizeof(float);
    printf("n_floats = %d\n", n_floats);

    Array<u32> quads = mesh->quads;
    if (!quads) {
        assert(mesh->tris);
        u32 *tris = mesh->tris.data;
        for (int itri = 0; itri < mesh->tris.len() / 6; itri++) {
            u32 t0_A = tris[itri * 6 + 0]; // A
            u32 t0_B = tris[itri * 6 + 1]; // B
            u32 t0_C = tris[itri * 6 + 2]; // C
            u32 t1_A = tris[itri * 6 + 3]; // A
            u32 t1_C = tris[itri * 6 + 4]; // C
            u32 t1_D = tris[itri * 6 + 5]; // D

            if (t0_A == t1_A && t0_C == t1_C) {
                quads += t0_A;
                quads += t0_B;
                quads += t1_C;
                quads += t1_D;
            } else if (t0_B == t1_A && t0_C == t1_D) {
                quads += t0_A;
                quads += t0_B;
                quads += t0_C;
                quads += t1_C;
            } else {
                assert(false);
            }
        }
    }



    int   n_input_verts = mesh->verts.count();
    int   n_input_quads = quads.len() / 4; /// must be made if it doesnt exist

    struct VFloats {
        float f_data[MAX_VERTEX_SIZE];
        VFloats() { }
        VFloats(VFloats const & src) { memcpy(f_data, src.f_data, n_floats * sizeof(float)); }
        void Clear(void *varg = 0)   { memset(f_data, 0, n_floats * sizeof(float)); }
        void AddWithWeight(VFloats const & src, float weight) {
            for (int i = 0; i < n_floats; i++)
                f_data[i] += weight * src.f_data[i];
        }
    };

    int  level_count = max_level - start_level + 1;
    bool subdiv = level_count != 1 || start_level != 0;
    Far::TopologyRefiner* refiner = null;
    VFloats  *verts = null;
    int *verts_per_face = null;

    if (subdiv) {
        using Descriptor = Far::TopologyDescriptor;
        Sdc::SchemeType type = OpenSubdiv::Sdc::SCHEME_CATMARK;
        Sdc::Options options;
        //options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

        verts_per_face = new int[n_input_quads];
        for (int i = 0; i < n_input_quads; i++)
            verts_per_face[i] = 4;
        
        Descriptor desc;
        desc.numVertices        = n_input_verts;
        desc.numFaces           = n_input_quads;
        desc.numVertsPerFace    = verts_per_face;
        desc.vertIndicesPerFace = (const i32*)quads.data;

        // Instantiate a Far::TopologyRefiner from the descriptor
        refiner = Far::TopologyRefinerFactory<Descriptor>::Create(desc,
                  Far::TopologyRefinerFactory<Descriptor>::Options(type, options));

        // uniformly refine the topology up to 'max_level'
        refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(max_level));

        // allocate a buffer for vertex primvar data.
        int n_total_max = refiner->GetNumVerticesTotal();
        Array<VFloats> vbuffer(n_total_max);
        verts = &vbuffer[0];

        // Initialize coarse mesh positions
        for (int i=0; i< mesh->verts.count(); ++i) {
            float *float_src = &f_origin[i * n_floats];
            memcpy(verts[i].f_data, float_src, n_floats * sizeof(float));
        }

        // Interpolate vertex primvar data
        Far::PrimvarRefiner primvarRefiner(*refiner);
        VFloats* src = verts;
        for (int level = 1; level <= max_level; ++level) {
            int level_verts = refiner->GetLevel(level-1).GetNumVertices();
            printf("level_verts at level %d = %d\n", level-1, level_verts);
            VFloats * dst = src + level_verts;
            primvarRefiner.Interpolate(level, src, dst);
            src = dst;
        }
    }

    /// copy verts from start of the level, for level_vcount
    bool quad = modes.contains(Polygon::quad);
    bool tri  = modes.contains(Polygon::tri);
    bool wire = modes.contains(Polygon::wire);
    
    Array<Mesh> results(level_count);

    for (int level = start_level; level <= max_level; level++) {
        bool subdiv = level > 0;
        Mesh m;
        m->level = level;
        Far::TopologyLevel const *last_level = 
                            subdiv ? &refiner -> GetLevel(level)   : null;
        int  level_vcount = subdiv ? last_level->GetNumVertices()  : mesh->verts.count();
        int  level_qcount = subdiv ? last_level->GetNumFaces()     : quads.len() / 4;

        /// copy verts (or) simply reference the mesh verts
        m->verts = (subdiv) ? memory::alloc(vtype, 0, level_vcount, null) : mesh->verts; /// todo: Array<T> type should simply be T; we already have the model of array
        if (subdiv) {
            float *new_origin = m->verts.origin<float>();
            int  level_origin = (refiner->GetNumVerticesTotal() - level_vcount);
            for (int i = 0; i < level_vcount; ++i) {
                float *dst = &new_origin[i * n_floats];
                float *src = verts[level_origin + i].f_data;
                memcpy(dst, src, n_floats * sizeof(float));
            }
            m->verts.set_size(level_vcount);
        }
        
        /// copy quads, tris, and wire
        assert(modes);
        if (quad) m->quads = memory::alloc(typeof(u32), 0, level_qcount * 4 * 1, null);
        if (tri)  m->tris  = memory::alloc(typeof(u32), 0, level_qcount * 3 * 2, null);
        if (wire) m->wire  = memory::alloc(typeof(u32), 0, level_qcount * 2 * 4, null);
        Far::ConstIndexArray fverts;
        for (int q = 0; q < level_qcount; ++q) { // all refined Catmark faces should be quads
            u32 *fv;
            if (subdiv) {
                fverts = last_level->GetFaceVertices(q);
                fv     = (u32*)&fverts[0]; /// assert this is contiguous
            } else {
                fv = &quads[q * 4];
            }
            if (quad) {
                m->quads[q * 4 + 0] = fv[0];
                m->quads[q * 4 + 1] = fv[1];
                m->quads[q * 4 + 2] = fv[2];
                m->quads[q * 4 + 3] = fv[3];
            }
            if (tri) {
                m->tris[q * 6 + 0] = fv[0];
                m->tris[q * 6 + 1] = fv[1];
                m->tris[q * 6 + 2] = fv[2];
                m->tris[q * 6 + 3] = fv[2];
                m->tris[q * 6 + 4] = fv[3];
                m->tris[q * 6 + 5] = fv[0];
            }
            if (wire) {
                m->wire[q * 8 + 0] = fv[0];
                m->wire[q * 8 + 1] = fv[1];
                m->wire[q * 8 + 2] = fv[1];
                m->wire[q * 8 + 3] = fv[2];
                m->wire[q * 8 + 4] = fv[2];
                m->wire[q * 8 + 5] = fv[3];
                m->wire[q * 8 + 6] = fv[3];
                m->wire[q * 8 + 7] = fv[0];
            }
        }
        if (quad) m->quads.set_size(level_qcount * 4);
        if (tri)  m->tris .set_size(level_qcount * 3 * 2);
        if (wire) m->wire .set_size(level_qcount * 4 * 2);
        results += m;
    }

    if (refiner)        delete refiner;
    if (verts_per_face) delete verts_per_face;
    
    return results;
}

const Mesh &Mesh::select(Array<Mesh> &meshes, int level) {
    for (Mesh &m: meshes) {
        if (m->level == level)
            return m;
    }
    console.fault("mesh not found for level of detail: {0}", { level });
    static Mesh null;
    return null;
}

}