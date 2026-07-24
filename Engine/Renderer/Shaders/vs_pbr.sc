$input a_position, a_color0, a_normal, a_texcoord0, a_weight, a_indices
$output v_position, v_color0, v_normal, v_texcoord0, v_posLightSpace

#include <bgfx_shader.sh>

uniform mat4 u_boneTransforms[64];
uniform mat4 u_lightMtx;

void main() {
    float totalWeight = a_weight[0] + a_weight[1] + a_weight[2] + a_weight[3];
    vec4 localPos = vec4(a_position, 1.0);
    vec4 localNorm = vec4(a_normal, 0.0);
    
    if (totalWeight > 0.01) {
        mat4 boneTransform = u_boneTransforms[int(a_indices[0])] * a_weight[0] +
                             u_boneTransforms[int(a_indices[1])] * a_weight[1] +
                             u_boneTransforms[int(a_indices[2])] * a_weight[2] +
                             u_boneTransforms[int(a_indices[3])] * a_weight[3];
        
        localPos = mul(boneTransform, localPos);
        localNorm = mul(boneTransform, localNorm);
    }

    vec3 wpos = mul(u_model[0], localPos).xyz;
    gl_Position = mul(u_viewProj, vec4(wpos, 1.0));
    v_position = wpos;
    v_posLightSpace = mul(u_lightMtx, vec4(wpos, 1.0));
    v_color0 = a_color0;
    v_normal = mul(u_model[0], localNorm).xyz;
    v_texcoord0 = a_texcoord0;
}
