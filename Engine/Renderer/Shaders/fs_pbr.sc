$input v_position, v_color0, v_normal

#include <bgfx_shader.sh>

uniform vec4 u_albedoRoughness;
uniform vec4 u_metallicEmissive;

void main() {
    vec3 N = normalize(v_normal);
    vec3 L = normalize(vec3(1.0, 1.0, -1.0));
    
    float ndotl = max(0.0, dot(N, L));
    // Çok basit bir diffuse shading + vertex color harmanlaması
    vec3 color = u_albedoRoughness.xyz * v_color0.xyz * (ndotl * 0.8 + 0.2);
    
    gl_FragColor = vec4(color, 1.0);
}
