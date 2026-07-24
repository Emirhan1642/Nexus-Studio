$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0); // HDR Scene Color
SAMPLER2D(s_texBloom, 1); // Blurred Bloom Color
uniform vec4 u_tonemapParams; // x = exposure, y = bloom intensity

// ACES Filmic Tone Mapping approximation
vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e), 0.0, 1.0);
}

void main() {
    vec3 hdrColor = texture2D(s_texColor, v_texcoord0).rgb;
    vec3 bloomColor = texture2D(s_texBloom, v_texcoord0).rgb;
    
    // Additive combine bloom
    hdrColor += bloomColor * u_tonemapParams.y;
    
    // Exposure
    hdrColor *= u_tonemapParams.x;
    
    // Tonemapping
    vec3 ldrColor = ACESFilm(hdrColor);
    
    // Gamma correction (sRGB approximation)
    ldrColor = pow(ldrColor, vec3_splat(1.0 / 2.2));
    
    gl_FragColor = vec4(ldrColor, 1.0);
}
