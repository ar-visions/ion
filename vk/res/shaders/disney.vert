#version 450

#define MAX_PBR_LIGHTS 3

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec3 in_normal;
layout(location = 2) in  vec2 in_uv;

layout(location = 0) out vec3 out_pos;
layout(location = 1) out vec2 out_uv;
layout(location = 2) out mat3 out_tbn;

void main() {

    /// transpose inverted model view of normal when making bi/tan/normal
    vec3 iN = mat3(transpose(inverse(ubo.model))) * in_normal;
    vec3 iU = mat3(transpose(ubo.model)) * vec3(0.0, 0.0, 1.0);
    vec3  T = cross(iN, iU);
    vec3  B = cross(T, iN);
    
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(in_pos, 1.0);

    out_pos     = (ubo.model * vec4(in_pos, 1.0)).xyz;
    out_uv      = in_uv;
    out_tbn     = mat3(T, B, iN);
}