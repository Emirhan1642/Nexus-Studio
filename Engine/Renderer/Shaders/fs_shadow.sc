$input v_pos

#include <bgfx_shader.sh>

void main() {
    // If using D16, this might be optional or we can pack depth into a color buffer (RGBA8).
    // For now we will rely on hardware depth buffer, so fragment shader can be very minimal.
    // However bgfx requires at least one output, or it's just writing to depth only.
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
