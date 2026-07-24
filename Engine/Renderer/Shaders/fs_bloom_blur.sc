$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_blurParams; // xy = texel offset * direction, e.g. [1.0/width, 0.0] or [0.0, 1.0/height]

void main() {
    vec2 uv = v_texcoord0;
    vec2 offset = u_blurParams.xy;
    
    vec3 result = vec3_splat(0.0);
    result += texture2D(s_texColor, uv - offset * 4.0).rgb * 0.016216;
    result += texture2D(s_texColor, uv - offset * 3.0).rgb * 0.054054;
    result += texture2D(s_texColor, uv - offset * 2.0).rgb * 0.1216216;
    result += texture2D(s_texColor, uv - offset * 1.0).rgb * 0.1945946;
    result += texture2D(s_texColor, uv).rgb * 0.227027;
    result += texture2D(s_texColor, uv + offset * 1.0).rgb * 0.1945946;
    result += texture2D(s_texColor, uv + offset * 2.0).rgb * 0.1216216;
    result += texture2D(s_texColor, uv + offset * 3.0).rgb * 0.054054;
    result += texture2D(s_texColor, uv + offset * 4.0).rgb * 0.016216;
    
    gl_FragColor = vec4(result, 1.0);
}
