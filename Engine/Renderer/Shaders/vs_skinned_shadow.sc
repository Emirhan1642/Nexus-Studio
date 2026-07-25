$input a_position, a_indices, a_weight
$output v_pos

#include <bgfx_shader.sh>

uniform mat4 u_boneTransforms[64];

void main() {
    mat4 boneTransform = 
        u_boneTransforms[int(a_indices[0])] * a_weight[0] +
        u_boneTransforms[int(a_indices[1])] * a_weight[1] +
        u_boneTransforms[int(a_indices[2])] * a_weight[2] +
        u_boneTransforms[int(a_indices[3])] * a_weight[3];

    vec4 localPos = mul(boneTransform, vec4(a_position, 1.0));
    vec3 wpos = mul(u_model[0], localPos).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    v_pos = gl_Position.xyz;
}
