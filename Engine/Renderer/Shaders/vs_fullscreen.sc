$input a_position, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

void main() {
    // Köşeler zaten NDC koordinatlarında (-1,-1 -> 1,1), transform gerekmez
    gl_Position = vec4(a_position, 1.0);
    v_texcoord0 = a_texcoord0;
}
