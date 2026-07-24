$input v_pos

#include <bgfx_shader.sh>

uniform vec4 u_lodParams;

void main() {
    if (u_lodParams.x < 0.999) {
        float dither = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
        if (u_lodParams.x < dither) {
            discard;
        }
    }
    
    // If using D16, this might be optional or we can pack depth into a color buffer (RGBA8).
    // For now we will rely on hardware depth buffer, so fragment shader can be very minimal.
    // However bgfx requires at least one output, or it's just writing to depth only.
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
