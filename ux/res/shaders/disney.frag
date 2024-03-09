#version 450

#define MAX_PBR_LIGHTS 1

struct Light {
    vec4 pos;
    vec4 color;
};

layout(binding = 0) uniform UniformBufferObject {
    mat4  model;
    mat4  view;
    mat4  proj;
    vec3  eye;
    Light lights[MAX_PBR_LIGHTS];
} ubo;

layout(location = 0) in vec3 v_pos; /// v_pos here is out_pos in .vert
layout(location = 1) in vec2 v_uv;
layout(location = 2) in mat3 v_tbn;

layout(location = 0) out vec4 pixel;

layout(binding = 1) uniform sampler2D tx_color;
layout(binding = 2) uniform sampler2D tx_normal;
layout(binding = 3) uniform sampler2D tx_material;
layout(binding = 4) uniform sampler2D tx_reflect;

const float Pi = 3.14159265359;

// compute fresnel specular factor for given base specular and product
// product could be NdV or VdH depending on used technique
vec3 fresnel_factor(vec3 f0, float product) {
    return mix(vec3(0), vec3(1.0), pow(1.01 - product, 5.0));
}

float D_beckmann(float roughness, float NdH) {
    float m = roughness * roughness;
    float m2 = m * m;
    float NdH2 = NdH * NdH;
    return exp((NdH2 - 1.0) / (m2 * NdH2)) / (Pi * m2 * NdH2 * NdH2);
}

float G_schlick(float roughness, float NdV, float NdL) {
    float k = roughness * roughness * 0.5;
    float V = NdV * (1.0 - k) + k;
    float L = NdL * (1.0 - k) + k;
    return 0.25 / (V * L);
}

void main() {
    vec4 color      = texture(tx_color, v_uv).rgba;
    vec3 albedo     = pow(color.xyz, vec3(2.2));
    
    vec3 normal_map = normalize(texture(tx_normal, v_uv).rgb * 2.0 - 1.0);
    vec3 N          = normalize(v_tbn * normal_map);

    float metallic  = texture(tx_material, v_uv).r * 2.0;
    float roughness = texture(tx_material, v_uv).g * 1.0;
    float ao        = texture(tx_material, v_uv).b * 1.0;

    // L,V,H vectors
    vec3 diffL = ubo.lights[0].pos.xyz - v_pos;
    vec3  L = normalize(diffL);
    vec3  V = normalize(ubo.eye - v_pos);
    vec3  H = normalize(L + V);
    vec3  R = reflect(V, N);

    float distance    = length(ubo.lights[0].pos.xyz - v_pos);
    float attenuation = (ubo.lights[0].pos.w * ubo.lights[0].pos.w) / (distance * distance);
    vec3  radiance    = ubo.lights[0].color.rgb * attenuation;

    // compute material reflectance
    float NdL = max(0.000, dot(N, L));
    float NdV = max(0.001, dot(N, V));
    float NdH = max(0.001, dot(N, H));
    float HdV = max(0.001, dot(H, V));
    //float LdV = max(0.001, dot(L, V));

    // set constant F0;
    vec3 F0  = vec3(0.04);
    F0       = mix(F0, albedo, metallic);

    float NDF = D_beckmann(roughness,NdH);
    vec3  F  = fresnel_factor(F0, HdV); // NdV or HdV?
    float G  = G_schlick(roughness, NdV, NdL);
    vec3  kS = F;
    vec3  kD = vec3(1.0) - kS;
    kD      *= 1.0 - metallic;

    vec3  numerator   = NDF * G * F;
    float denominator = 4.0 * NdV * NdL;
    vec3  specular    = numerator / max(denominator, 0.001);

    /// attempt to incorporate reflection map
    /*
    float lon     = atan(R.z, R.x);
    float lat     = asin(R.y);
    vec2  r_uv    = vec2(lon, lat) / (2.0 * Pi) + 0.5;
    vec3  r_color = texture(tx_reflect, r_uv).rgb;
    vec3 diff_color = albedo * (1.0 - metallic);
    vec3 spec_color = mix(r_color,    albedo,     roughness);
    vec3 l_color    = mix(diff_color, spec_color, metallic);
    vec3 f_color    = l_color * F;
    */
    
    vec3 Lo         = (kD * albedo / Pi + specular) * NdL * radiance;
    vec3 ambient    = vec3(0.03) * albedo * ao;
    vec3 result     = ambient + Lo;
  
    result          = result / (result + vec3(1.0));
    result          = pow(result, vec3(1.0 / 2.2));

    pixel           = vec4(vec3(result), 1.0);
}