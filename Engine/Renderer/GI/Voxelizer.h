#pragma once
#include <bgfx/bgfx.h>
#include <vector>
#include "../SceneGraph/RenderProxy.h"

namespace Engine::Renderer {

class Voxelizer {
public:
    Voxelizer();
    ~Voxelizer();

    void init();
    void shutdown();
    
    // Sahneyi voxel grid'ine yazar
    void voxelizeScene(const std::vector<RenderProxy>& proxies, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh);

    bgfx::TextureHandle getVoxelTexture() const { return m_voxelTexture; }

private:
    static constexpr int VOXEL_RESOLUTION = 256;
    bgfx::TextureHandle m_voxelTexture = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_voxelizationProgram = BGFX_INVALID_HANDLE;
    
    // Viewport ID for voxelization pass
    static constexpr bgfx::ViewId View_Voxelize = 10;
};

} // namespace Engine::Renderer
