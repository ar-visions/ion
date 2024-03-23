
#include <trinity/subdiv.hpp>

using namespace ion;

namespace ion {

struct Vertex:mx {
    struct M {
        float*          coords;
        float*          normals;    //3d coordinates etc
        int             idx;        //who am i; verts[idx]
        array<int>      vertList;   //adj vertices
        array<int>      triList;
        array<int>      quadList;
        array<int>      edgeList;
        register(M)
    };
    Vertex(int idx, float* c) : Vertex() {
        data->idx = idx;
        data->coords = c; // compatible with our mx vertex, atleast for position
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
        register(M)
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
        register(M)
    };
	Triangle(int idx, int v1i, int v2i, int v3i) : Triangle() {
        data->id = idx;
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
        register(M)
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
    }

    void import(array<mx> &verts, array<u32> &quads) {

    }

    void addQuad(int v1, int v2, int v3, int v4) {
        int idx = quads.len();
        quads.push_back(Quad(idx, v1, v2, v3, v4));
        ////printf("Add quad called for %d , %d , %d , %d\n", v1, v2, v3, v4);

        if (!makeVertsNeighbor(v1, v2))
            addEdge(v1, v2, idx);
        else {
            int temp = getEdgeFromVerts(v1, v2);
            //printf("These verts %d and %d already edge in edge: %d \n",v1,v2, temp);
            if (temp != -1) {
                if (std::count(edges[temp]->quadIdx.begin(), edges[temp]->quadIdx.end(), idx) == 0) {
                    edges[temp]->quadIdx.push_back(idx);
                    ////printf("Edge %d has new quad: %d \n", temp, idx);
                }
            }
        }

        if (!makeVertsNeighbor(v2, v3))
            addEdge(v2, v3, idx);
        else {
            int temp = getEdgeFromVerts(v2, v3);
            ////printf("These verts %d and  %d are already edge in edge: %d \n",v2,v3, temp);
            if (temp != -1) {
                if (std::count(edges[temp]->quadIdx.begin(), edges[temp]->quadIdx.end(), idx) == 0) {
                    edges[temp]->quadIdx.push_back(idx);
                    ////printf("Edge %d has new quad: %d \n", temp, idx);
                }
            }
        }

        if (!makeVertsNeighbor(v3, v4))
            addEdge(v3, v4, idx);
        else {
            int temp = getEdgeFromVerts(v3, v4);
            ////printf("These verts %d and %d are already edge in edge: %d \n",v3,v4, temp);
            if (temp != -1) {
                if (std::count(edges[temp]->quadIdx.begin(), edges[temp]->quadIdx.end(), idx) == 0) {
                    edges[temp]->quadIdx.push_back(idx);
                    ////printf("Edge %d has new quad: %d \n", temp, idx);
                }
            }
        }

        if (!makeVertsNeighbor(v1, v4))
            addEdge(v1, v4, idx);
        else {
            ////printf(" %d , %d are already neighbour!\n", v1, v4);
            int temp = getEdgeFromVerts(v1, v4);
            ////printf("These verts %d and %d already edge in edge: %d \n",v1,v4, temp);
            if (temp != -1) {
                if (std::count(edges[temp]->quadIdx.begin(), edges[temp]->quadIdx.end(), idx) == 0)
                {
                    edges[temp]->quadIdx.push_back(idx);
                    ////printf("Edge %d has new quad: %d \n", temp, idx);
                }
            }
        }

        verts[v1]->quadList.push_back(idx);
        verts[v2]->quadList.push_back(idx);
        verts[v3]->quadList.push_back(idx);
        verts[v4]->quadList.push_back(idx);
    }

    void addCatmullClarkQuad(int v1, int v2, int v3, int v4, vector<Quad*>& cmQuads, vector<Edge*> &cmEdges, vector<int>& verts_oldEdgeList) {
        int idx = cmQuads.len();
        cmQuads.push_back(Quad(idx, v1, v2, v3, v4));
        //printf("Quad will be created with verts %d , %d , %d , %d", v1, v2, v3, v4);
        if (!makeVertsNeighbor(v1, v2))
            addCatmullClarkEdge(v1, v2, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v1, v2,cmEdges, verts_oldEdgeList);
            if (temp != -1) {
                //printf("edge %d will be added new quad %d\n", temp, idx);
                cmEdges[temp]->quadIdx.push_back(idx);
            }
        }
        if (!makeVertsNeighbor(v2, v3))
            addCatmullClarkEdge(v2, v3, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v2, v3, cmEdges, verts_oldEdgeList);
            if (temp != -1) {
                //printf("edge %d will be added new quad %d\n", temp, idx);
                cmEdges[temp]->quadIdx.push_back(idx);
                //printf("edge %d added new quad %d\n", temp, idx);
            }
        }
        if (!makeVertsNeighbor(v3, v4))
            addCatmullClarkEdge(v3, v4, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v3, v4, cmEdges, verts_oldEdgeList);
            if (temp != -1) {
                //printf("edge %d will be added new quad %d\n", temp, idx);
                cmEdges[temp]->quadIdx.push_back(idx);
                //printf("edge %d added new quad %d\n", temp, idx);
            }
        }
        if (!makeVertsNeighbor(v1, v4))
            addCatmullClarkEdge(v1, v4, cmEdges, idx);
        else {
            int temp = getCatmullClarkEdgeFromVerts(v1, v4, cmEdges, verts_oldEdgeList);
            if (temp != -1) {
                //printf("edge %d will be added new quad %d\n", temp, idx);
                cmEdges[temp]->quadIdx.push_back(idx);
                //printf("edge %d added new quad %d\n", temp, idx);
            }
        }

        verts[v1]->quadList.push_back(idx);
        verts[v2]->quadList.push_back(idx);
        verts[v3]->quadList.push_back(idx);
        verts[v4]->quadList.push_back(idx);
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
        if (quads.len() < 1) {
            //printf("Cant perform Catmull-Clark subdivision on non-quad meshes");
            return;
        }

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

            tempVert[0] = (verts[quads[i]->v1i]->coords[0] + verts[quads[i]->v2i]->coords[0]
                + verts[quads[i]->v3i]->coords[0] + verts[quads[i]->v4i]->coords[0]) / 4.0f;

            tempVert[1] = (verts[quads[i]->v1i]->coords[1] + verts[quads[i]->v2i]->coords[1]
                + verts[quads[i]->v3i]->coords[1] + verts[quads[i]->v4i]->coords[1]) / 4.0f;

            tempVert[2] = (verts[quads[i]->v1i]->coords[2] + verts[quads[i]->v2i]->coords[2]
                + verts[quads[i]->v3i]->coords[2] + verts[quads[i]->v4i]->coords[2]) / 4.0f;

            addVertex(tempVert[0], tempVert[1], tempVert[2]);
            //////printf("Face point coordinats %.5g, %.5g, %.5g\n", tempVert[0], tempVert[1], tempVert[2]);

            //to decrease complexity we will store facepoint idx of each quad
            quads[i]->facePointIdx = verts.len() - 1;
        }

        //create edge points for each edge
        ////printf("Creating edge points!\n");

        /*  _______E1______ 
            |      |      |
            |  F1  ep  F2 |  Edge Point: ep = (F1+F2+E1+E2)\4
            |______|______|
                E2	
        */
        for (int i = 0; i < edges.len(); i++) {
            if (edges[i]->quadIdx.len() != 2) {
                //printf("This mesh NOT watertight and edges[%d] is at boundary.\n",i);
                edges[i]->midPointIdx = -1;
                continue;
            }

            //printf("edges[%d]->quadIdx.len() = %d\n",i, edges[i]->quadIdx.len());
            //printf("edges[%d]->quadIdx[0] = %d\n",i, edges[i]->quadIdx[0]);
            //printf("edges[%d]->quadIdx[1] = %d\n",i, edges[i]->quadIdx[1]);
            

            tempVert[0] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[0] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[0] +
                verts[edges[i]->v1i]->coords[0] +
                verts[edges[i]->v2i]->coords[0]) / 4.0f;

            tempVert[1] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[1] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[1] +
                verts[edges[i]->v1i]->coords[1] +
                verts[edges[i]->v2i]->coords[1]) / 4.0f;

            tempVert[2] = (verts[quads[edges[i]->quadIdx[0]]->facePointIdx]->coords[2] +
                verts[quads[edges[i]->quadIdx[1]]->facePointIdx]->coords[2] +
                verts[edges[i]->v1i]->coords[2] +
                verts[edges[i]->v2i]->coords[2]) / 4.0f;
            ////printf("Edge point coordinats %.5g, %.5g, %.5g\n", tempVert[0], tempVert[1], tempVert[2]);
            addVertex(tempVert[0], tempVert[1], tempVert[2]);
            edges[i]->midPointIdx = verts.len() - 1;

        }

        //printf("*********Moving the old vertices now!\n");
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
            if (verts[i]->quadList.len() != verts[i]->edgeList.len()) {
                //printf("EdgeList.len() of %d th vertex is %d\n", i, n);
                //printf("QuadList.len() of %d th vertex is %d\n\n", i, verts[i]->quadList.len());
                continue;
            }

            //printf("Moving %d th vertex\n", i);
            n = verts[i]->edgeList.len();
            //printf("EdgeList.len() of %d th vertex is %d\n", i, n);
            //printf("QuadList.len() of %d th vertex is %d\n", i, verts[i]->quadList.len());
            tempX_fp = 0.0f;
            tempX_ep = 0.0f;

            tempY_fp = 0.0f;
            tempY_ep = 0.0f;

            tempZ_fp = 0.0f;
            tempZ_ep = 0.0f;
            for (int j = 0; j < n; j++) {
                //printf("quads.len() = %d\n", quads.len());
                //printf("verts[%d]->quadList[%d] = %d\n", i, j, verts[i]->quadList[j]);

                //printf("edges.len() = %d\n", edges.len());
                //printf("verts[%d]->edgeList[%d] = %d\n", i, j, verts[i]->edgeList[j]);

                tempX_fp += verts[quads[verts[i]->quadList[j]]->facePointIdx]->coords[0]/n;
                tempX_ep += verts[edges[verts[i]->edgeList[j]]->midPointIdx]->coords[0] / n;

                tempY_fp += verts[quads[verts[i]->quadList[j]]->facePointIdx]->coords[1] / n;
                tempY_ep += verts[edges[verts[i]->edgeList[j]]->midPointIdx]->coords[1] / n;

                tempZ_fp += verts[quads[verts[i]->quadList[j]]->facePointIdx]->coords[2] / n;
                tempZ_ep += verts[edges[verts[i]->edgeList[j]]->midPointIdx]->coords[2] / n;
            }
            tempVert[0] = ((float)tempX_fp / n) + ((float)2.0f * tempX_ep / n) + ((float)(n - 3) * verts[i]->coords[0] / (n));

            tempVert[1] = ((float)tempY_fp / n) + ((float)2.0f * tempY_ep / n) + ((float)(n - 3) * verts[i]->coords[1] / (n));

            tempVert[2] = ((float)tempZ_fp / n) + ((float)2.0f * tempZ_ep / n) + ((float)(n - 3) * verts[i]->coords[2] / (n));

            verts[i]->coords[0] = tempVert[0];
            verts[i]->coords[1] = tempVert[1];
            verts[i]->coords[2] = tempVert[2];

            
        }
        
        //printf("*********We will create new quads and edges now!\n");


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

        for (int i = 0; i < oldVertsIdx+1; i++) {
            verts_oldEdges.push_back(verts[i]->edgeList.len());
        }

        int tempEdge1_2, tempEdge2_3, tempEdge3_4, tempEdge4_1;
        int tempQuadIdx=-1, tempEdge1=-1, tempEdge2=-1;
        int vert1_oldQuads, vert1_oldEdges, vert2_oldQuads, vert2_oldEdges,
            vert3_oldQuads, vert3_oldEdges, vert4_oldQuads, vert4_oldEdges;
        
        for (int i = 0; i < quads.len(); i++) {
            //printf("%d th quad\n", i);
            //printf("edges.len() = %d\n", edges.len());
            tempEdge1_2 = getEdgeFromVerts(quads[i]->v1i, quads[i]->v2i);
            //printf("tempEdge1_2 = %d\n", tempEdge1_2);
            tempEdge2_3 = getEdgeFromVerts(quads[i]->v2i, quads[i]->v3i);
            //printf("tempEdge2_3 = %d\n", tempEdge2_3);

            tempEdge3_4 = getEdgeFromVerts(quads[i]->v3i, quads[i]->v4i);
            //printf("tempEdge3_4 = %d\n", tempEdge3_4);

            tempEdge4_1 = getEdgeFromVerts(quads[i]->v4i, quads[i]->v1i);
            //printf("tempEdge4_1 = %d\n", tempEdge4_1);
            
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
        //printf("\n\n We will erase old verts \n\n");
        for (int i = 0; i < oldVertsIdx + 1; i++) {
            verts[i]->edgeList.erase(verts[i]->edgeList.begin(), verts[i]->edgeList.begin() +
                verts_oldEdges[i] );
        }
        
        copyEdges(catmullEdges);
        ////printf("catmullEdges.len() = %d\n", catmullEdges.len());
        copyQuads(catmullQuads);
        catmullEdges.clear();
        catmullQuads.clear();
        delete[] tempVert;
        ////printf("******Catmull Clark Subdivision Completed!\n");
    }
};

mx_implement(Mesh, IMesh)

Mesh Mesh::import_vbo(mx vbo, mx ibo, bool convert_from_tris) {
    Mesh mesh;
    mesh->vtype = vbo.type();

    assert(ibo.type() == typeof(u32));

    if (convert_from_tris) {
        u32 *tris = ibo.origin<u32>();
        array<u32> &quads = mesh->quads;

        for (int itri = 0; itri < ibo.count() / 6; itri++) {
            u32 t0_A = tris[itri * 6 + 0]; // A
            u32 t0_B = tris[itri * 6 + 1]; // B
            u32 t0_C = tris[itri * 6 + 2]; // C
            u32 t1_A = tris[itri * 6 + 3]; // A
            u32 t1_C = tris[itri * 6 + 4]; // C
            u32 t1_D = tris[itri * 6 + 5]; // D

            if (t0_A == t1_A && t0_C == t1_C) {
                assert(t0_A == t1_A && t0_C == t1_C);
                quads += Quad(t0_A, t0_B, t1_C, t1_D);
            } else if (t0_B == t1_A && t0_C == t1_D) {
                quads += Quad(t0_A, t0_B, t0_C, t1_C);
                assert(t0_B == t1_A && t0_C == t1_D);
            } else {
                debug_break(); // never happens (blender model exported to glTF (their exporter does not alter indexing data))
            }
        }
    } else {
        mesh->quads = ibo.grab();
    }

    /// copy vertex data (no altering original)
    type_t vtype  = vbo.type();
    num    stride = vtype->base_sz;
    u8*    vdata  = vbo.origin<u8>();

    mesh->verts = array<mx>(vbo.count() * 4); /// reserve enough space for copy and sub-division
    for (num v = 0; v < vbo.count(); i++)
        mesh->verts += memory::alloc(vtype, 1, 1, &vdata[stride * v]);
}

void Mesh::export_vbo(mx &vbo, mx &ibo, bool convert_to_tris) {
    /// simple tessellation from quads (maybe TOO simple!)
    auto tessellate = [](array<u32> &quads) {
        array<u32> tris(quads.len() / 4 * 6);
        for (int i = 0; i < quads.len(); i += 4) {
            tris += quads[i + 0];
            tris += quads[i + 1];
            tris += quads[i + 2];
            tris += quads[i + 3];
            tris += quads[i + 0];
            tris += quads[i + 2];
        }
        return tris.hold();
    };

    /// convert ibo buffer to triangles from quads
    ibo        = convert_to_tris ? tessellate(data->quads) : data->quads.hold();
    
    vbo        = memory::alloc(data->vtype, data->verts.len(), 0, null);
    u8* dst    = vbo.origin<u8>();
    num stride = vtype->base_sz;
    
    for (num v = 0; v < vbo.count(); i++) {
        u8* src = data->verts[v].origin<u8>();
        memcpy(&dst[v * stride], src, stride);
    }
}

/// @github/bertaye wrote very clear code on this
/// leveraging that work with mx meta api; uses any input Vertex type containing a meta map
void Mesh::catmull_clark() {
    data->cutmull_clark();
}

}