$input v_position, v_color0, v_normal, v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_albedoRoughness;
uniform vec4 u_metallicEmissive;
uniform vec4 u_textureFlags;

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texNormal, 1);
SAMPLER2D(s_texMetallic, 2);
SAMPLER2D(s_texRoughness, 3);

void main() {
    vec3 albedo = u_albedoRoughness.xyz * v_color0.xyz;
    if (u_textureFlags.x > 0.5) {
        albedo = texture2D(s_texColor, v_texcoord0).xyz * u_albedoRoughness.xyz;
    }

    vec3 N = normalize(v_normal);
    if (u_textureFlags.y > 0.5) {
        vec3 normalMap = texture2D(s_texNormal, v_texcoord0).xyz * 2.0 - 1.0;
        N = normalize(N + normalMap * 0.5); // Simple perturbation since we don't have TBN
    }

    float metallic = u_metallicEmissive.x;
    if (u_textureFlags.z > 0.5) {
        metallic = texture2D(s_texMetallic, v_texcoord0).x * u_metallicEmissive.x;
    }

    float roughness = u_albedoRoughness.w;
    if (u_textureFlags.w > 0.5) {
        roughness = texture2D(s_texRoughness, v_texcoord0).x * u_albedoRoughness.w;
    }

    vec3 L = normalize(vec3(1.0, 1.0, -1.0));
    float ndotl = max(0.0, dot(N, L));
    
    // Çok basit bir diffuse shading
    vec3 color = albedo * (ndotl * 0.8 + 0.2);
    
    gl_FragColor = vec4(color, 1.0);
}
