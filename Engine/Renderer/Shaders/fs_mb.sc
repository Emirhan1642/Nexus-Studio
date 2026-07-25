$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texDepth, 1);

uniform vec4 u_mbParams;
uniform mat4 u_prevViewProj;

void main() {
    float depth = texture2D(s_texDepth, v_texcoord0).x;
    
#if BGFX_SHADER_LANGUAGE_GLSL
    vec4 ndc = vec4(v_texcoord0.x * 2.0 - 1.0, (1.0 - v_texcoord0.y) * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
#else
    vec4 ndc = vec4(v_texcoord0.x * 2.0 - 1.0, (1.0 - v_texcoord0.y) * 2.0 - 1.0, depth, 1.0);
#endif

    vec4 worldPos = mul(u_invViewProj, ndc);
    worldPos /= worldPos.w;

    vec4 prevNdc = mul(u_prevViewProj, worldPos);
    prevNdc /= prevNdc.w;

    vec2 prevUV = prevNdc.xy * 0.5 + 0.5;
    prevUV.y = 1.0 - prevUV.y; 

    vec2 velocity = (v_texcoord0 - prevUV) * u_mbParams.x;

    float len = length(velocity);
    if (len > 0.05) {
        velocity = (velocity / len) * 0.05;
    }

    vec4 color = texture2D(s_texColor, v_texcoord0);
    
    int numSamples = 8;
    for (int i = 1; i < 8; ++i) {
        vec2 offset = velocity * (float(i) / 7.0 - 0.5);
        color += texture2D(s_texColor, v_texcoord0 + offset);
    }
    
    gl_FragColor = color / 8.0;
}
