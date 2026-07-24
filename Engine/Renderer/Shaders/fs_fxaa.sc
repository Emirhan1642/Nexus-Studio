$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texTonemap, 0);
uniform vec4 u_fxaaParams; // x: 1/width, y: 1/height

#define FXAA_EDGE_THRESHOLD_MIN 0.0312
#define FXAA_EDGE_THRESHOLD_MAX 0.125
#define FXAA_SUBPIX_TRIM (1.0/4.0)

float FxaaLuma(vec3 rgb) {
    return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec2 texcoord = v_texcoord0;
    vec2 invTexSize = u_fxaaParams.xy;
    
    vec3 rgbM  = texture2D(s_texTonemap, texcoord).xyz;
    vec3 rgbNW = texture2D(s_texTonemap, texcoord + vec2(-1.0, -1.0) * invTexSize).xyz;
    vec3 rgbNE = texture2D(s_texTonemap, texcoord + vec2( 1.0, -1.0) * invTexSize).xyz;
    vec3 rgbSW = texture2D(s_texTonemap, texcoord + vec2(-1.0,  1.0) * invTexSize).xyz;
    vec3 rgbSE = texture2D(s_texTonemap, texcoord + vec2( 1.0,  1.0) * invTexSize).xyz;
    
    float lumaM  = FxaaLuma(rgbM);
    float lumaNW = FxaaLuma(rgbNW);
    float lumaNE = FxaaLuma(rgbNE);
    float lumaSW = FxaaLuma(rgbSW);
    float lumaSE = FxaaLuma(rgbSE);
    
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    float lumaRange = lumaMax - lumaMin;
    
    if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD_MAX)) {
        gl_FragColor = vec4(rgbM, 1.0);
        return;
    }
    
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_SUBPIX_TRIM), FXAA_EDGE_THRESHOLD_MIN);
    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2(8.0, 8.0), max(vec2(-8.0, -8.0), dir * rcpDirMin)) * invTexSize;
    
    vec3 rgbA = (1.0/2.0) * (
        texture2D(s_texTonemap, texcoord + dir * (1.0/3.0 - 0.5)).xyz +
        texture2D(s_texTonemap, texcoord + dir * (2.0/3.0 - 0.5)).xyz);
        
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        texture2D(s_texTonemap, texcoord + dir * (0.0/3.0 - 0.5)).xyz +
        texture2D(s_texTonemap, texcoord + dir * (3.0/3.0 - 0.5)).xyz);
        
    float lumaB = FxaaLuma(rgbB);
    
    if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
        gl_FragColor = vec4(rgbA, 1.0);
    } else {
        gl_FragColor = vec4(rgbB, 1.0);
    }
}
