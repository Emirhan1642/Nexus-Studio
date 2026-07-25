$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texHistory, 1);
SAMPLER2D(s_texDepth, 2);

uniform vec4 u_taaParams;
uniform mat4 u_prevViewProj;

void main() {
    vec2 uv = v_texcoord0;
    vec4 currentColor = texture2D(s_texColor, uv);
    float depth = texture2D(s_texDepth, uv).x;

    if (depth >= 0.99999) {
        gl_FragColor = currentColor;
        return;
    }

    vec4 clipPos = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
#if !BGFX_SHADER_LANGUAGE_GLSL
    clipPos.y = -clipPos.y;
#endif

    vec4 worldPos = mul(u_invViewProj, clipPos);
    worldPos /= worldPos.w;

    vec4 prevClipPos = mul(u_prevViewProj, worldPos);
    prevClipPos /= prevClipPos.w;

    vec2 prevUV = prevClipPos.xy * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
    prevUV.y = 1.0 - prevUV.y;
#endif

    // Neighborhood Clamping
    // Approximate texel size (Ideally passed via uniform, but this works for MVP)
    vec2 texelSize = vec2(1.0 / 1280.0, 1.0 / 720.0); 
    
    vec4 c0 = texture2D(s_texColor, uv + vec2(-1.0, -1.0) * texelSize);
    vec4 c1 = texture2D(s_texColor, uv + vec2( 0.0, -1.0) * texelSize);
    vec4 c2 = texture2D(s_texColor, uv + vec2( 1.0, -1.0) * texelSize);
    vec4 c3 = texture2D(s_texColor, uv + vec2(-1.0,  0.0) * texelSize);
    vec4 c4 = currentColor;
    vec4 c5 = texture2D(s_texColor, uv + vec2( 1.0,  0.0) * texelSize);
    vec4 c6 = texture2D(s_texColor, uv + vec2(-1.0,  1.0) * texelSize);
    vec4 c7 = texture2D(s_texColor, uv + vec2( 0.0,  1.0) * texelSize);
    vec4 c8 = texture2D(s_texColor, uv + vec2( 1.0,  1.0) * texelSize);

    vec4 minColor = min(min(min(min(min(min(min(min(c0, c1), c2), c3), c4), c5), c6), c7), c8);
    vec4 maxColor = max(max(max(max(max(max(max(max(c0, c1), c2), c3), c4), c5), c6), c7), c8);

    vec4 historyColor = texture2D(s_texHistory, prevUV);
    historyColor = clamp(historyColor, minColor, maxColor);

    float blendFactor = u_taaParams.z;
    if (prevUV.x < 0.0 || prevUV.x > 1.0 || prevUV.y < 0.0 || prevUV.y > 1.0) {
        blendFactor = 1.0;
    }

    gl_FragColor = mix(historyColor, currentColor, blendFactor);
}
