$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 u_bloomParams; // x = threshold, y = unused, z = unused, w = unused

void main() {
    vec3 color = texture2D(s_texColor, v_texcoord0).rgb;
    
    // Calculate brightness
    float brightness = max(color.r, max(color.g, color.b));
    float contribution = max(0.0, brightness - u_bloomParams.x);
    contribution /= max(brightness, 0.00001);
    
    gl_FragColor = vec4(color * contribution, 1.0);
}
