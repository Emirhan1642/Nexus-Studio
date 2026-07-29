$input a_position, a_color0, a_normal, a_texcoord0
$output v_position, v_color0, v_normal, v_texcoord0, v_viewDepth

#include <bgfx_shader.sh>

void main() {
    vec4 localPos  = vec4(a_position, 1.0);
    vec4 localNorm = vec4(a_normal,   0.0);

    vec3 wpos      = mul(u_model[0], localPos).xyz;
    gl_Position    = mul(u_viewProj, vec4(wpos, 1.0));

    v_position     = wpos;
    v_viewDepth    = mul(u_view, vec4(wpos, 1.0)).z;
    v_color0       = a_color0;
    v_normal       = mul(u_model[0], localNorm).xyz;
    v_texcoord0    = a_texcoord0;
}
