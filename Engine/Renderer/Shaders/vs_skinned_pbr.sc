$input a_position, a_color0, a_normal, a_texcoord0, a_weight, a_indices
$output v_position, v_color0, v_normal, v_texcoord0, v_posLightSpace

#include <bgfx_shader.sh>

// Uniform buffer for bone matrices (Max 64 bones for MVP)
uniform mat4 u_boneMatrices[64];
uniform mat4 u_lightMtx;

void main() {
    mat4 skinMatrix = 
        a_weight[0] * u_boneMatrices[int(a_indices[0])] +
        a_weight[1] * u_boneMatrices[int(a_indices[1])] +
        a_weight[2] * u_boneMatrices[int(a_indices[2])] +
        a_weight[3] * u_boneMatrices[int(a_indices[3])];

    vec3 skinnedPosition = mul(skinMatrix, vec4(a_position, 1.0)).xyz;
    vec3 skinnedNormal = mul(skinMatrix, vec4(a_normal, 0.0)).xyz;
    
    vec3 wpos = mul(u_model[0], vec4(skinnedPosition, 1.0)).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    
    v_position = wpos;
    v_posLightSpace = mul(u_lightMtx, vec4(wpos, 1.0));
    v_color0 = a_color0;
    v_normal = mul(u_model[0], vec4(skinnedNormal, 0.0)).xyz;
    v_texcoord0 = a_texcoord0;
}
