$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);       // SSGI Color
SAMPLER2D(s_texNormalGBuffer, 1); // Normal + Roughness (Alpha)
SAMPLER2D(s_texDepth, 2);       // Depth

uniform vec4 u_ssrParams; // x = max steps, y = step size, z = roughness threshold, w = thickness

// Reconstruct view-space position from depth
vec3 reconstructViewPos(vec2 uv, float depth) {
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
#if BGFX_SHADER_LANGUAGE_GLSL
    // nothing
#else
    clipPos.y = -clipPos.y; // D3D texture coordinates
#endif
    vec4 viewPos = mul(u_invProj, clipPos);
    return viewPos.xyz / viewPos.w;
}

void main() {
    float depth = texture2D(s_texDepth, v_texcoord0).x;
    
    // Background/Sky Check
    if (depth >= 0.99999) {
        gl_FragColor = texture2D(s_texColor, v_texcoord0);
        return;
    }

    vec4 normalData = texture2D(s_texNormalGBuffer, v_texcoord0);
    vec3 normal = normalize(normalData.xyz * 2.0 - 1.0);
    float roughness = normalData.w;

    vec4 originalColor = texture2D(s_texColor, v_texcoord0);

    // If surface is too rough, skip SSR
    if (roughness > u_ssrParams.z) {
        gl_FragColor = originalColor;
        return;
    }

    vec3 viewPos = reconstructViewPos(v_texcoord0, depth);
    vec3 viewDir = normalize(viewPos);
    
    // Calculate reflection vector in view space
    vec3 reflectionDir = normalize(reflect(viewDir, normal));

    float maxSteps = u_ssrParams.x;
    float stepSize = u_ssrParams.y;
    float thickness = u_ssrParams.w;

    vec3 rayPos = viewPos;
    float hit = 0.0;
    vec2 hitUV = vec2(0.0, 0.0);

    for (int i = 1; i <= 20; ++i) { // maxSteps is max 20 for loop unrolling bounds
        if (float(i) > maxSteps) break;

        rayPos += reflectionDir * stepSize;

        // Project ray pos to screen
        vec4 rayClip = mul(u_proj, vec4(rayPos, 1.0));
        vec2 rayUV = rayClip.xy / rayClip.w;
        rayUV = rayUV * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
        rayUV.y = 1.0 - rayUV.y;
#endif

        if (rayUV.x < 0.0 || rayUV.x > 1.0 || rayUV.y < 0.0 || rayUV.y > 1.0) {
            break; // Ray went off screen
        }

        float sampleDepth = texture2D(s_texDepth, rayUV).x;
        vec3 samplePos = reconstructViewPos(rayUV, sampleDepth);

        float distDiff = abs(rayPos.z) - abs(samplePos.z);

        if (distDiff > 0.0 && distDiff < thickness) {
            hit = 1.0;
            hitUV = rayUV;
            break;
        }
    }

    if (hit > 0.5) {
        vec3 reflectionColor = texture2D(s_texColor, hitUV).rgb;
        
        // Edge fade
        vec2 edgeFade = smoothstep(0.0, 0.1, hitUV) * smoothstep(1.0, 0.9, hitUV);
        float fade = edgeFade.x * edgeFade.y;
        
        // Fresnel approximation for mix factor
        float NdotV = max(dot(normal, -viewDir), 0.0);
        float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - NdotV, 5.0);
        
        // Roughness fade
        float roughnessFade = 1.0 - (roughness / u_ssrParams.z);
        
        float mixFactor = fade * fresnel * roughnessFade;
        
        vec3 finalColor = mix(originalColor.rgb, reflectionColor, clamp(mixFactor, 0.0, 1.0));
        gl_FragColor = vec4(finalColor, 1.0);
    } else {
        gl_FragColor = originalColor;
    }
}
