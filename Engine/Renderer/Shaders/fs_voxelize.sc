$input v_position, v_normal

#include <bgfx_shader.sh>

#ifndef IMAGE3D_WR
#if BGFX_SHADER_LANGUAGE_HLSL
#define IMAGE3D_WR(_name, _format, _reg) RWTexture3D<unorm float4> _name : register(u ## _reg)
#elif BGFX_SHADER_LANGUAGE_GLSL || BGFX_SHADER_LANGUAGE_VULKAN
#define IMAGE3D_WR(_name, _format, _reg) layout(_format, binding=_reg) writeonly uniform highp image3D _name
#else
#define IMAGE3D_WR(_name, _format, _reg)
#endif

#ifndef imageStore
#if BGFX_SHADER_LANGUAGE_HLSL
#define imageStore(_name, _coord, _data) _name[_coord] = _data
#endif
#endif
#endif

IMAGE3D_WR(s_voxelGrid, rgba8, 1);

void main() {
    // Map world pos [-50, 50] to [0, 1] then to voxel coordinates [0, 255]
    vec3 normalizedPos = (v_position + vec3(50.0, 50.0, 50.0)) / 100.0;
    
    if (normalizedPos.x < 0.0 || normalizedPos.x > 1.0 ||
        normalizedPos.y < 0.0 || normalizedPos.y > 1.0 ||
        normalizedPos.z < 0.0 || normalizedPos.z > 1.0) {
        discard;
    }
    
    ivec3 voxelCoord = ivec3(normalizedPos * 255.0);
    
    // Store white color and 1.0 opacity for solid geometry
    vec4 voxelData = vec4(1.0, 1.0, 1.0, 1.0);
    
    imageStore(s_voxelGrid, voxelCoord, voxelData);
    
    // Since we need to write to framebuffer or something if this is not a pure compute/UAV-only pass in some older GL versions,
    // we can output dummy color to a tiny framebuffer, but BGFX handles compute-only fragment shaders fine if bound to no attachments.
    // However, to satisfy compiler, we output to gl_FragColor.
    gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
