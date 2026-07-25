$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
SAMPLER2D(s_texDepth, 1);

uniform vec4 u_dofParams;

void main() {
    float focusDist = u_dofParams.x;
    float focalRange = u_dofParams.y;
    float maxBlur = u_dofParams.z;

    float depth = texture2D(s_texDepth, v_texcoord0).x;
    float near = 0.1;
    float far = 1000.0;
    
#if BGFX_SHADER_LANGUAGE_GLSL
    float z_n = 2.0 * depth - 1.0;
    float linDepth = 2.0 * near * far / (far + near - z_n * (far - near));
#else
    float linDepth = near * far / (far - depth * (far - near));
#endif

    float coc = clamp(abs(linDepth - focusDist) / focalRange, 0.0, 1.0) * maxBlur;

    vec4 color = texture2D(s_texColor, v_texcoord0);
    vec2 texel = vec2(1.0 / 1920.0, 1.0 / 1080.0);
    
    if (coc > 0.01) {
        vec2 poisson[8];
        poisson[0] = vec2(-0.86,  0.50);
        poisson[1] = vec2(-0.15, -0.98);
        poisson[2] = vec2( 0.45, -0.89);
        poisson[3] = vec2( 0.98,  0.15);
        poisson[4] = vec2( 0.65,  0.75);
        poisson[5] = vec2(-0.25,  0.96);
        poisson[6] = vec2(-0.95, -0.30);
        poisson[7] = vec2(-0.55, -0.15);
        
        vec4 sum = color;
        for (int i=0; i<8; ++i) {
            sum += texture2D(s_texColor, v_texcoord0 + poisson[i] * texel * coc * 30.0);
        }
        color = sum / 9.0;
    }

    gl_FragColor = color;
}
