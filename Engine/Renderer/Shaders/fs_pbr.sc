$input v_position, v_color0, v_normal, v_texcoord0, v_posLightSpace

#include <bgfx_shader.sh>

uniform vec4 u_albedoRoughness;
uniform vec4 u_metallicEmissive;
uniform vec4 u_textureFlags;

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texNormal, 1);
SAMPLER2D(s_texMetallic, 2);
SAMPLER2D(s_texRoughness, 3);
SAMPLER2D(s_texShadow, 4);

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
    
    // Gölgeleme (Shadow Mapping) Hesaplaması
    vec3 projCoords = v_posLightSpace.xyz / v_posLightSpace.w;
    
    // Homojen koordinatları [0, 1] aralığına çevir
    // bgfx (DirectX / OpenGL / Vulkan) için y ekseni ayarı:
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
#if BGFX_SHADER_LANGUAGE_GLSL
    // OpenGL uses -1..1 for Z
    projCoords.z = projCoords.z * 0.5 + 0.5;
#endif

    float shadow = 0.0;
    
    // Frustum dışında kalan yerlerde gölge yok say
    if (projCoords.x >= 0.0 && projCoords.x <= 1.0 && 
        projCoords.y >= 0.0 && projCoords.y <= 1.0 &&
        projCoords.z >= 0.0 && projCoords.z <= 1.0) {
        
        vec2 texelSize = vec2(1.0 / 2048.0, 1.0 / 2048.0);
        float bias = 0.002;
        
        // 3x3 PCF
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture2D(s_texShadow, projCoords.xy + vec2(x, y) * texelSize).x;
                // If light space depth is greater than shadow map depth, it's in shadow
                shadow += projCoords.z - bias > pcfDepth ? 0.0 : 1.0;
            }
        }
        shadow /= 9.0;
    } else {
        shadow = 1.0; // Dışarısı aydınlık
    }
    
    // Ambient + Diffuse (Gölgeyle çarpılarak)
    vec3 color = albedo * (ndotl * 0.8 * shadow + 0.2);
    
    gl_FragData[0] = vec4(color, 1.0);
    gl_FragData[1] = vec4(N * 0.5 + 0.5, 1.0); // Pack normal to [0, 1]
}
