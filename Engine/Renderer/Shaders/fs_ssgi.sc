$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);       // HDR Color
SAMPLER2D(s_texNormalGBuffer, 1); // Normal
SAMPLER2D(s_texDepth, 2);       // Depth

uniform vec4 u_ssgiParams; // x = radius, y = intensity, z = ssao intensity, w = resolution scale (unused)

// Reconstruct view-space position from depth
vec3 reconstructViewPos(vec2 uv, float depth) {
    // Get clip space coordinates
    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
#if BGFX_SHADER_LANGUAGE_GLSL
    // nothing
#else
    clipPos.y = -clipPos.y; // D3D texture coordinates
#endif
    vec4 viewPos = mul(u_invProj, clipPos);
    return viewPos.xyz / viewPos.w;
}

// Random noise generator based on UV
float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    float depth = texture2D(s_texDepth, v_texcoord0).x;
    if (depth >= 0.99999) {
        // Sky or background
        gl_FragColor = texture2D(s_texColor, v_texcoord0);
        return;
    }

    vec3 viewPos = reconstructViewPos(v_texcoord0, depth);
    
    // Normal was packed [0, 1]. Unpack it.
    vec3 normal = texture2D(s_texNormalGBuffer, v_texcoord0).xyz * 2.0 - 1.0;
    
    // Simple SSAO + SSGI logic
    float radius = u_ssgiParams.x;
    int samples = 8;
    float ao = 0.0;
    vec3 indirectColor = vec3(0.0, 0.0, 0.0);
    
    float noise = rand(v_texcoord0 * 100.0);
    
    for (int i = 0; i < samples; ++i) {
        // Generate a random sample point in a hemisphere
        float r1 = rand(vec2(noise, float(i)));
        float r2 = rand(vec2(float(i), noise));
        
        vec3 sampleDir = normalize(vec3(r1 * 2.0 - 1.0, r2 * 2.0 - 1.0, rand(vec2(r1, r2))));
        if (dot(sampleDir, normal) < 0.0) {
            sampleDir = -sampleDir;
        }
        
        // Push sample point along normal and sample direction
        vec3 samplePos = viewPos + sampleDir * radius;
        
        // Project to clip space to get UV
        vec4 offsetClip = mul(u_proj, vec4(samplePos, 1.0));
        vec2 offsetUV = offsetClip.xy / offsetClip.w;
        offsetUV = offsetUV * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
        offsetUV.y = 1.0 - offsetUV.y;
#endif

        if (offsetUV.x >= 0.0 && offsetUV.x <= 1.0 && offsetUV.y >= 0.0 && offsetUV.y <= 1.0) {
            float sampleDepth = texture2D(s_texDepth, offsetUV).x;
            vec3 reconstructedSamplePos = reconstructViewPos(offsetUV, sampleDepth);
            
            // Check range
            float rangeCheck = smoothstep(0.0, 1.0, radius / abs(viewPos.z - reconstructedSamplePos.z));
            
            // Check if occluded
            if (reconstructedSamplePos.z < samplePos.z - 0.02) {
                ao += 1.0 * rangeCheck;
                
                // SSGI: if occluded, it means a surface is there. Add its color as indirect bounce.
                vec3 bounceColor = texture2D(s_texColor, offsetUV).rgb;
                indirectColor += bounceColor * rangeCheck;
            }
        }
    }
    
    ao = 1.0 - (ao / float(samples)) * u_ssgiParams.z;
    ao = clamp(ao, 0.0, 1.0);
    
    indirectColor = (indirectColor / float(samples)) * u_ssgiParams.y;
    
    vec3 finalColor = texture2D(s_texColor, v_texcoord0).rgb * ao + indirectColor;
    
    gl_FragColor = vec4(finalColor, 1.0);
}
