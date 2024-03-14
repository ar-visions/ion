
#include <trinity/subdiv.hpp>

using namespace ion;

glm::vec3 average_verts(face& f, array<glm::vec3>& verts) {
    glm::vec3 sum(0.0f);
    for (size_t i = 0; i < f.size; ++i)
        sum += verts[f.indices[i]];
    return sum / r32(f.size);
}

mesh subdiv(mesh& input_mesh, array<glm::vec3>& verts) {
    mesh sdiv_mesh;
    std::unordered_map<vpair, glm::vec3, vpair_hash> face_points;
    std::unordered_map<vpair, glm::vec3, vpair_hash> edge_points;
    array<glm::vec3> new_verts = verts;
    const size_t vlen = verts.len();

    /// Compute face points
    for (face& f: input_mesh) {
        glm::vec3 face_point = average_verts(f, verts);
        for (u32 i = 0; i < f.size; ++i) {
            int vertex_index1 = f.indices[i];
            int vertex_index2 = f.indices[(i + 1) % f.size];
            vpair edge(vertex_index1, vertex_index2);
            face_points[edge] = face_point;
        }
    }

    /// Compute edge points
    for (auto& fp: face_points) {
        const  vpair &edge = fp.first;
        glm::vec3       midpoint = (verts[edge.first] + verts[edge.second]) / 2.0f;
        glm::vec3 face_point_avg = (face_points[edge] + face_points[vpair(edge.second, edge.first)]) / 2.0f;
        ///
        edge_points[edge]  = (midpoint + face_point_avg) / 2.0f;
    }

    /// Update original verts
    for (size_t i = 0; i < verts.len(); ++i) {
        glm::vec3 sum_edge_points(0.0f);
        glm::vec3 sum_face_points(0.0f);
        u32 n = 0;

        for (const auto& ep : edge_points) {
            vpair edge = ep.first;
            if (edge.first == i || edge.second == i) {
                sum_edge_points += ep.second;
                sum_face_points += face_points[edge];
                n++;
            }
        }

        glm::vec3 avg_edge_points = sum_edge_points / r32(n);
        glm::vec3 avg_face_points = sum_face_points / r32(n);
        glm::vec3 original_vertex = verts[i];

        new_verts[i] = (avg_face_points + avg_edge_points * 2.0f + original_vertex * (n - 3.0f)) / r32(n);
    }

    /// create new faces
    for (const face& f : input_mesh) {
        
        /// for each vertex in face
        for (u32 i = 0; i < f.size; ++i) {
            face new_face;
            new_face.size = 4;
            u32 new_indices[4];

            new_indices[0] = f.indices[i];
            new_indices[1] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[i], f.indices[(i + 1) % f.size]))));
            new_indices[2] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[(i + 1) % f.size], f.indices[(i + 2) % f.size]))));
            new_indices[3] = u32(vlen + std::distance(edge_points.begin(), edge_points.find(vpair(f.indices[(i + 2) % f.size], f.indices[i]))));

            new_face.indices = new_indices;
            sdiv_mesh       += new_face;
        }
    }

    return sdiv_mesh;
}

mesh subdiv_cube() {
    array<glm::vec3> verts = {
        // Front face
        { -0.5f, -0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },

        // Back face
        { -0.5f, -0.5f, -0.5f },
        { -0.5f,  0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },

        // Top face
        { -0.5f,  0.5f, -0.5f },
        { -0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f,  0.5f },
        {  0.5f,  0.5f, -0.5f },

        // Bottom face
        { -0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f, -0.5f },
        {  0.5f, -0.5f,  0.5f },
        { -0.5f, -0.5f,  0.5f },

        // Right face
        {  0.5f, -0.5f, -0.5f },
        {  0.5f,  0.5f, -0.5f },
        {  0.5f,  0.5f,  0.5f },
        {  0.5f, -0.5f,  0.5f },

        // Left face
        { -0.5f, -0.5f, -0.5f },
        { -0.5f, -0.5f,  0.5f },
        { -0.5f,  0.5f,  0.5f },
        { -0.5f,  0.5f, -0.5f }
    };

    mesh input_mesh = {
        /* Size: */ 24,
        /* Indices: */ {
            0,  1,  2,  0,  2,  3,  // Front face
            4,  5,  6,  4,  6,  7,  // Back face
            8,  9,  10, 8,  10, 11, // Top face
            12, 13, 14, 12, 14, 15, // Bottom face
            16, 17, 18, 16, 18, 19, // Right face
            20, 21, 22, 20, 22, 23  // Left face
        }
    };

    return subdiv(input_mesh, verts);
}