#include <mx/mx.hpp>
#include <trinity/subdiv.hpp>

namespace ion {

struct Vertex:mx {
    struct M {
        float*          coords;
        float*          normals;    //3d coordinates etc
        int             idx;        //who am i; verts[idx]
        mx              vdata;      // data for entire vertex, in vtype
        array<int>      vertList;   //adj vertices
        array<int>      triList;
        array<int>      quadList;
        array<int>      edgeList;
    };
    Vertex(int idx, mx vdata) : Vertex() {
        data->idx = idx;
        data->vdata = vdata;
        data->coords = vdata.origin<float>();
    }
    /// interpolate an average using meta properties
    Vertex(int idx, mx (&src)[4]) : Vertex() {
        type_t vtype = src[0].type(); /// all must be same, of course
        data->idx    = idx;
        data->vdata  = memory::alloc(vtype, 1, 0, src[0].origin<void>());
        data->coords = data->vdata.origin<float>();
        /// we want to add by T and divide by count==4
    }
    mx_basic(Vertex)
};

struct Edge:mx {
    struct M {
        int idx; //edges[idx]
        array<int> quadIdx; //an edge consists of 2 quads, we need them for calculating
                            //edge point on catmull-clark algorithm
        int midPointIdx; //mid point of the edge, will be needed for catmull-clark
        int v1i, v2i; //endpnts
        float length;
    };
	Edge(int idx, int v1i, int v2i) : Edge() {
        data->idx = idx;
        data->v1i = v1i;
        data->v2i = v2i;
    }
    mx_basic(Edge)
};

struct Triangle:mx {
    struct M {
        int idx; //tris[idx]
        int midPointIdx;
        int v1i, v2i, v3i;
    };
	Triangle(int idx, int v1i, int v2i, int v3i) : Triangle() {
        data->idx = idx;
        data->v1i = v1i;
        data->v2i = v2i;
        data->v3i = v3i;
    };
    mx_basic(Triangle)
};

struct Quad:mx {
    struct M {
        int idx;
        int facePointIdx = -1;
        int v1i, v2i, v3i, v4i;
    };
	Quad(int idx, int v1i, int v2i, int v3i, int v4i) : Quad() {
        data->idx = idx;
        data->v1i = v1i;
        data->v2i = v2i;
        data->v3i = v3i;
        data->v4i = v4i;
    };
    mx_basic(Quad)
};

struct IMesh {
    type_t            vtype;
    array<Vertex>     verts; /// needs to preserve all attributes; just hold onto mx. (will need proper interpolation across the attribs)
    array<Triangle>   tris;
    array<Edge>       edges;
    array<Quad>       quads;

    /// using for Reference for our import
    /*
    void loadOffQuadMesh(const char* name) {
        FILE* fPtr;
        fopen_s(&fPtr,name, "r");
        char str[334];

        fscanf_s(fPtr, "%s", str,334);

        int nVerts, nQuads, n, i = 0;
        float x, y, z, w;

        fscanf_s(fPtr, "%d %d %d\n", &nVerts, &nQuads, &n);
        while (i++ < nVerts)
        {
            fscanf_s(fPtr, "%f %f %f", &x, &y, &z);
            addVertex(x, y, z);
        }

        while (fscanf_s(fPtr, "%d", &i) != EOF)
        {
            fscanf_s(fPtr, "%f %f %f %f", &x, &y, &z, &w); /// reading floats from index data!
            addQuad((int)x, (int)y, (int)z, (int)w);
            
            makeVertsNeighbor(x, y);
            makeVertsNeighbor(y, z);
            makeVertsNeighbor(z, w);
            makeVertsNeighbor(w, x);
        }

        fclose(fPtr);
    }*/

    bool makeVertsNeighbor(int v1i, int v2i) {
        //returns true if v1i already neighbor w/ v2i; false o/w
        for (int i = 0; i < verts[v1i]->vertList.len(); i++)
            if (verts[v1i]->vertList[i] == v2i)
                return true;

        Vertex &v1 = verts[v1i];
        v1->vertList += v2i;
        verts[v2i]->vertList += v1i;
        return false;
    }

    void addVertex(const mx &vert) {
        int idx = verts.len();
        verts += Vertex(idx, vert);
    }

    void addVertex(const mx (&v4)[4]) {
        int idx = verts.len();
        verts += Vertex(idx, v4);
    }

    /// dont use this.
    void addVertex(int ph, float x, float y, float z) { /// needs to take in an entire mx vertex 
        int idx = verts.len();
        float* c = (float*)calloc(vtype->base_sz, 1);
        c[0] = x;
        c[1] = y;
        c[2] = z;
        verts += Vertex(idx, memory::alloc(vtype, 3, 0, c));
    }

    int getEdgeFromVerts(int v1, int v2) {
        for (int ei: verts[v1]->edgeList) {
            Edge &e = edges[ei];
            if ((e->v1i == v1 && e->v2i == v2) ||
                (e->v1i == v2 && e->v2i == v1))
                return ei;
        }
        return -1;
        /*for (int i = 0; i < verts[v1]->edgeList.len(); i++) {
            if (edges[verts[v1]->edgeList[i]]->v1i == v1 && edges[verts[v1]->edgeList[i]]->v2i == v2 ||
                edges[verts[v1]->edgeList[i]]->v1i == v2 && edges[verts[v1]->edgeList[i]]->v2i == v1)
                return verts[v1]->edgeList[i];
        }
        return -1;*/
    }

    void addQuad(int v1, int v2, int v3, int v4) {
        int idx = quads.len();
        quads += Quad(idx, v1, v2, v3, v4);
        if (!makeVertsNeighbor(v1, v2))
            addEdge(v1, v2, idx);
        else {
            int temp = getEdgeFromVerts(v1, v2);
            if (temp != -1 && !edges[temp]->quadIdx.contains(idx))
                edges[temp]->quadIdx += idx;
        }

        if (!makeVertsNeighbor(v2, v3))
            addEdge(v2, v3, idx);
        else {
            int temp = getEdgeFromVerts(v2, v3);
            if (temp != -1) {
                if (!edges[temp]->quadIdx.contains(idx))
                    edges[temp]->quadIdx += idx;
            }
        }

        if (!makeVertsNeighbor(v3, v4))
            addEdge(v3, v4, idx);
        else {
            int temp = getEdgeFromVerts(v3, v4);
            if (temp != -1) {
                if (!edges[temp]->quadIdx.contains(idx))
                    edges[temp]->quadIdx += idx;
            }
        }
        if (!makeVertsNeighbor(v1, v4))
            addEdge(v1, v4, idx);
        else {
            int temp = getEdgeFromVerts(v1, v4);
            if (temp != -1) {
                if (!edges[temp]->quadIdx.contains(idx))
                    edges[temp]->quadIdx += idx; /// Edge %d has new quad
            }
        }
        verts[v1]->quadList += idx;
        verts[v2]->quadList += idx;
        verts[v3]->quadList += idx;
        verts[v4]->quadList += idx;
    }

    void addEdge(int v1, int v2, int quadIdx) {
        int idx = edges.len();
        edges += Edge(idx, v1, v2);
        if (quadIdx != -1)
            edges[idx]->quadIdx += quadIdx;
        verts[v1]->edgeList += idx;
        verts[v2]->edgeList += idx;
    }

    void addCatmullClarkEdge(int v1, int v2 , array<Edge>& cmEdges, int quadIdx) {
        int idx = cmEdges.len();
        //printf("catmull edge index with %d created between: %d and %d with QUAD: %d\n",idx, v1, v2,quadIdx);
        cmEdges += Edge(idx, v1, v2);
        cmEdges[idx]->quadIdx += quadIdx;
        
        verts[v1]->edgeList += idx;
        verts[v2]->edgeList += idx;
    }

    int getCatmullClarkEdgeFromVerts(int v1, int v2, array<Edge>& cmEdge, array<int>& verts_oldEdgeList) {
        if (v1 < verts_oldEdgeList.len()) {
            for (int i = verts_oldEdgeList[v1]; i < verts[v1]->edgeList.len(); i++)
                for (int j = 0; j < verts[v2]->edgeList.len(); j++)
                    if (verts[v1]->edgeList[i] == verts[v2]->edgeList[j])
                        return verts[v1]->edgeList[i];
        }
        else if (v2 < verts_oldEdgeList.len()) {
            for (int i = 0; i < verts[v1]->edgeList.len(); i++)
                for (int j = verts_oldEdgeList[v2]; j < verts[v2]->edgeList.len(); j++)
                    if (verts[v1]->edgeList[i] == verts[v2]->edgeList[j])
                        return verts[v1]->edgeList[i];
        }
        else {
            for (int i = 0; i < verts[v1]->edgeList.len(); i++)
                for (int j = 0; j < verts[v2]->edgeList.len(); j++)
                    if (verts[v1]->edgeList[i] == verts[v2]->edgeList[j])
                        return verts[v1]->edgeList[i];
        }
        return -1;
    }

    void addCatmullClarkQuad(int v1, int v2, int v3, int v4, array<Quad>& cmQuads, array<Edge>& cmEdges, array<int>& verts_oldEdgeList) {
        int idx = cmQuads.len();
        cmQuads += Quad(idx, v1, v2, v3, v4);
        if (!makeVertsNeighbor(v1, v2))
            addCatmullClarkEdge(v1, v2, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v1, v2, cmEdges, verts_oldEdgeList);
            if (temp != -1)
                cmEdges[temp]->quadIdx += idx;
        }
        if (!makeVertsNeighbor(v2, v3))
            addCatmullClarkEdge(v2, v3, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v2, v3, cmEdges, verts_oldEdgeList);
            if (temp != -1)
                cmEdges[temp]->quadIdx += idx;
        }
        if (!makeVertsNeighbor(v3, v4))
            addCatmullClarkEdge(v3, v4, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v3, v4, cmEdges, verts_oldEdgeList);
            if (temp != -1)
                cmEdges[temp]->quadIdx += idx;
        }
        if (!makeVertsNeighbor(v1, v4))
            addCatmullClarkEdge(v1, v4, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v1, v4, cmEdges, verts_oldEdgeList);
            if (temp != -1)
                cmEdges[temp]->quadIdx += idx;
        }

        verts[v1]->quadList += idx;
        verts[v2]->quadList += idx;
        verts[v3]->quadList += idx;
        verts[v4]->quadList += idx;
    }

    void copyEdges(const array<Edge> edgev) {
        edges = edgev;
    }

    void copyQuads(const array<Quad> quadv) {
        quads = quadv;
    }

    void copyTriangles(const array<Triangle> triangleV) {
        tris = triangleV;
    }

    void catmull_clark() {
        float* tempVert = new float[3]; //[0]=x, [1]=y, [2]=z
        if (!quads) 
            return;

        //after creating new vertices we need to update coords of old verts.
        //we must know where should we stop.
        int oldVertsIdx = verts.len() - 1;

        ////printf("Creating face points!\n");
        //create face points for each quad
        /*
            A______B
            |  	   | 	
            |  fp  |     Face Point: fp = (A+B+C+D)/4
        C|______|D

        */
        for (int i = 0; i < quads.len(); i++) {
            Quad &q = quads[i];

            float *coords0 = verts[q->v1i]->coords;
            float *coords1 = verts[q->v2i]->coords;
            float *coords2 = verts[q->v3i]->coords;
            float *coords3 = verts[q->v4i]->coords;

            tempVert[0] = (verts[q->v1i]->coords[0] + verts[q->v2i]->coords[0]
                + verts[q->v3i]->coords[0] + verts[q->v4i]->coords[0]) / 4.0f;

            tempVert[1] = (verts[q->v1i]->coords[1] + verts[q->v2i]->coords[1]
                + verts[q->v3i]->coords[1] + verts[q->v4i]->coords[1]) / 4.0f;

            tempVert[2] = (verts[q->v1i]->coords[2] + verts[q->v2i]->coords[2]
                + verts[q->v3i]->coords[2] + verts[q->v4i]->coords[2]) / 4.0f;

            //todo
            //addVertex(tempVert[0], tempVert[1], tempVert[2]);
            q->facePointIdx = verts.len() - 1;
        }

        //create edge points for each edge
        /*  _______E1______ 
            |      |      |
            |  F1  ep  F2 |  Edge Point: ep = (F1+F2+E1+E2)\4
            |______|______|
                   E2	
        */
       ///
        for (int i = 0; i < edges.len(); i++) {
            if (edges[i]->quadIdx.len() != 2) {
                //printf("This mesh NOT water-tight and edges[%d] is at boundary.\n",i);
                edges[i]->midPointIdx = -1;
                continue;
            }
            mx v[4] = {
                verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->vdata,
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->vdata,
                verts[edges[i]->v1i]->vdata,
                verts[edges[i]->v2i]->vdata
            };
            addVertex(v);
            /*
            /// interpolate X
            tempVert[0] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[0] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[0] +
                verts[edges[i]->v1i]->coords[0] +
                verts[edges[i]->v2i]->coords[0]) / 4.0f;

            /// interpolate Y
            tempVert[1] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[1] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[1] +
                verts[edges[i]->v1i]->coords[1] +
                verts[edges[i]->v2i]->coords[1]) / 4.0f;

            /// interpolate Z
            tempVert[2] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[2] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[2] +
                verts[edges[i]->v1i]->coords[2] +
                verts[edges[i]->v2i]->coords[2]) / 4.0f;
            ////printf("Edge point coordinats %.5g, %.5g, %.5g\n", tempVert[0], tempVert[1], tempVert[2]);
            addVertex(tempVert[0], tempVert[1], tempVert[2]);
            */
            edges[i]->midPointIdx = verts.len() - 1;

        }

        //move old verts to their new positions
        /*
                    |
                    |
            fp1	    ep1    fp2
                    |
                    |
        __ep2_____v_____ep4______        fp: face point, ep: edge point
                    |                       
                    |                       v_new = (fp1+fp2+fp3+fp4)/4/n + 2*(ep1+ep2+ep3+ep4)/4/n + (n-3)v/n
            fp3 	ep3    fp4
                    |
                    |
        */
        float tempX_fp=-1.0f,tempX_ep = -1.0f, tempY_fp = -1.0f, tempY_ep = -1.0f, tempZ_fp = -1.0f, tempZ_ep = -1.0f;
        int n=-1;
        for (int i = 0; i < oldVertsIdx+1; i++) {
            Vertex &v = verts[i];
            if (v->quadList.len() != v->edgeList.len())
                continue;

            n = v->edgeList.len();
            tempX_fp = 0.0f;
            tempX_ep = 0.0f;

            tempY_fp = 0.0f;
            tempY_ep = 0.0f;

            tempZ_fp = 0.0f;
            tempZ_ep = 0.0f;

            for (int j = 0; j < n; j++) {
                tempX_fp += verts[quads[v->quadList[j]]->facePointIdx]->coords[0]/n;
                printf("v->edgeList[j] = %d\n", v->edgeList[j]);
                printf("edges len = %d\n", (int)edges.len());
                printf("midPointIdx = %d\n", edges[v->edgeList[j]]->midPointIdx);
                tempX_ep += verts[edges[v->edgeList[j]]->midPointIdx]->coords[0] / n;

                tempY_fp += verts[quads[v->quadList[j]]->facePointIdx]->coords[1] / n;
                tempY_ep += verts[edges[v->edgeList[j]]->midPointIdx]->coords[1] / n;

                tempZ_fp += verts[quads[v->quadList[j]]->facePointIdx]->coords[2] / n;
                tempZ_ep += verts[edges[v->edgeList[j]]->midPointIdx]->coords[2] / n;
            }
            tempVert[0] = ((float)tempX_fp / n) + ((float)2.0f * tempX_ep / n) + ((float)(n - 3) * v->coords[0] / (n));

            tempVert[1] = ((float)tempY_fp / n) + ((float)2.0f * tempY_ep / n) + ((float)(n - 3) * v->coords[1] / (n));
            tempVert[2] = ((float)tempZ_fp / n) + ((float)2.0f * tempZ_ep / n) + ((float)(n - 3) * v->coords[2] / (n));

            v->coords[0] = tempVert[0];
            v->coords[1] = tempVert[1];
            v->coords[2] = tempVert[2];
        }
 
        //now we need to create new quads and edges!
        array<Edge> catmullEdges;
        array<Quad> catmullQuads;

        //new quads will contain only 1 old vertex per quad!
        /*
            we will create quads by going through quadList of verties and then get the mutual edges of that quad & vertex
            because edges contain all information needed; -midpoint of edges and hence neighbouring midpoints to vertex! (ep1,ep2)
                                                        -quads of edges and hence neighbouring facepoints to vertex! (fp)
            and our new quad will be simply this-> v,ep1,fp,ep2
                                                        
        */
        array<int> verts_oldEdges;

        for (int i = 0; i < verts.len(); i++) {
            verts[i]->quadList.clear();
            verts[i]->vertList.clear();
        }

        for (int i = 0; i < oldVertsIdx+1; i++)
            verts_oldEdges += verts[i]->edgeList.len();

        int tempEdge1_2, tempEdge2_3, tempEdge3_4, tempEdge4_1;
        int tempQuadIdx=-1, tempEdge1=-1, tempEdge2=-1;
        int vert1_oldQuads, vert1_oldEdges, vert2_oldQuads, vert2_oldEdges,
            vert3_oldQuads, vert3_oldEdges, vert4_oldQuads, vert4_oldEdges;

        for (int i = 0; i < quads.len(); i++) {
            tempEdge1_2 = getEdgeFromVerts(quads[i]->v1i, quads[i]->v2i);
            tempEdge2_3 = getEdgeFromVerts(quads[i]->v2i, quads[i]->v3i);
            tempEdge3_4 = getEdgeFromVerts(quads[i]->v3i, quads[i]->v4i);
            tempEdge4_1 = getEdgeFromVerts(quads[i]->v4i, quads[i]->v1i);
            if (edges[tempEdge1_2]->midPointIdx == -1 || edges[tempEdge2_3]->midPointIdx == -1
                || edges[tempEdge3_4]->midPointIdx == -1 || edges[tempEdge4_1]->midPointIdx == -1) {
                continue;
            }
            addCatmullClarkQuad(quads[i]->v1i, edges[tempEdge1_2]->midPointIdx, 
                quads[i]->facePointIdx, edges[tempEdge4_1]->midPointIdx,catmullQuads,catmullEdges, verts_oldEdges);

            addCatmullClarkQuad(quads[i]->v2i, edges[tempEdge2_3]->midPointIdx,
                quads[i]->facePointIdx, edges[tempEdge1_2]->midPointIdx, catmullQuads, catmullEdges, verts_oldEdges);

            addCatmullClarkQuad(quads[i]->v3i, edges[tempEdge3_4]->midPointIdx,
                quads[i]->facePointIdx, edges[tempEdge2_3]->midPointIdx, catmullQuads, catmullEdges, verts_oldEdges);

            addCatmullClarkQuad(quads[i]->v4i, edges[tempEdge4_1]->midPointIdx,
                quads[i]->facePointIdx, edges[tempEdge3_4]->midPointIdx, catmullQuads, catmullEdges, verts_oldEdges);
        }

        // erase old verts
        //for (int i = 0; i < oldVertsIdx + 1; i++)
        //    verts[i]->edgeList->remove(0, verts_oldEdges[i]);
        
        copyEdges(catmullEdges);
        copyQuads(catmullQuads);
        catmullEdges.clear();
        catmullQuads.clear();
        delete[] tempVert;
    }

};

mx_implement(Mesh, mx, IMesh)

Mesh Mesh::import_vbo(mx vbo, mx ibo, bool convert_from_tris) {
    Mesh mesh;
    mesh->vtype = vbo.type();
    array<Quad> &quads = mesh->quads;

    /// copy vertex data (no altering original)
    type_t vtype  = vbo.type();
    num    stride = vtype->base_sz;
    u8*    vdata  = vbo.origin<u8>();

    mesh->verts = array<Vertex>(vbo.count() * 4); /// reserve enough space for copy and sub-division
    for (num v = 0; v < vbo.count(); v++) {
        mx velement(vtype, &vdata[v * stride]);
        mesh->verts += Vertex(v, velement);
    }
    if (convert_from_tris) {
        u32 *tris = ibo.origin<u32>();
        for (int itri = 0; itri < ibo.count() / 6; itri++) {
            u32 t0_A = tris[itri * 6 + 0]; // A
            u32 t0_B = tris[itri * 6 + 1]; // B
            u32 t0_C = tris[itri * 6 + 2]; // C
            u32 t1_A = tris[itri * 6 + 3]; // A
            u32 t1_C = tris[itri * 6 + 4]; // C
            u32 t1_D = tris[itri * 6 + 5]; // D

            if (t0_A == t1_A && t0_C == t1_C) {
                assert(t0_A == t1_A && t0_C == t1_C);
                mesh->addQuad(t0_A, t0_B, t1_C, t1_D);
                mesh->makeVertsNeighbor(t0_A, t0_B);
                mesh->makeVertsNeighbor(t0_B, t0_C);
                mesh->makeVertsNeighbor(t0_C, t1_D);
                mesh->makeVertsNeighbor(t1_D, t0_A);

            } else if (t0_B == t1_A && t0_C == t1_D) {
                mesh->addQuad(t0_A, t0_B, t0_C, t1_C);
                mesh->makeVertsNeighbor(t0_A, t0_B); /// these are not validated
                mesh->makeVertsNeighbor(t0_B, t0_C);
                mesh->makeVertsNeighbor(t0_C, t1_C);
                mesh->makeVertsNeighbor(t1_C, t0_A);
                assert(t0_B == t1_A && t0_C == t1_D);
            } else {
            }
        }
    } else {
        u32 *quads = ibo.origin<u32>();
        for (int iquad = 0; iquad < ibo.count() / 4; iquad++) {
            u32 qA = quads[iquad * 4 + 0]; // A
            u32 qB = quads[iquad * 4 + 1]; // B
            u32 qC = quads[iquad * 4 + 2]; // C
            u32 qD = quads[iquad * 4 + 3]; // D
            mesh->addQuad(qA, qB, qD, qC); // ABCD is a bow-tie layout from blender, so lets flip C and D, making a clock-wise Quad
        }
    }
    return mesh;
}

void Mesh::export_vbo(mx &vbo, mx &quads, mx &out_tris, bool convert_to_tris) {
    /// simple tessellation from quads (maybe TOO simple!)
    auto tessellate = [](array<Quad> &quads) {
        array<u32> tris(quads.len() / 4 * 6);
        for (Quad &quad: quads) {
            tris += quad->v1i;
            tris += quad->v2i;
            tris += quad->v3i;
            tris += quad->v3i;
            tris += quad->v4i;
            tris += quad->v1i;
        }
        return tris.hold();
    };

    /// convert ibo buffer to triangles from quads
    quads = data->quads.hold(); /// we can always use quads for rendering wireframe
    if (convert_to_tris)
        out_tris = tessellate(data->quads);

    vbo        = memory::alloc(data->vtype, data->verts.len(), 0, null);
    u8* dst    = vbo.origin<u8>();
    num stride = data->vtype->base_sz;
    
    for (num v = 0; v < vbo.count(); v++) {
        u8* src = data->verts[v]->vdata.origin<u8>();
        memcpy(&dst[v * stride], src, stride);
    }

    u32 *indices = quads.origin<u32>();

    glm::vec3 &v_t0_TL = *(glm::vec3*)&dst[indices[0] * stride];
    glm::vec3 &v_t0_TR = *(glm::vec3*)&dst[indices[1] * stride];
    glm::vec3 &v_t0_BR = *(glm::vec3*)&dst[indices[2] * stride];

    glm::vec3 &v_t1_BR = *(glm::vec3*)&dst[indices[3] * stride];
    glm::vec3 &v_t1_BL = *(glm::vec3*)&dst[indices[4] * stride];
    glm::vec3 &v_t1_TL = *(glm::vec3*)&dst[indices[5] * stride];

    debug_break();
}

/// @github/bertaye wrote very clear code on this
/// leveraging that work with mx meta api; uses any input Vertex type containing a meta map
void Mesh::catmull_clark() {
    data->catmull_clark();
}

}