#include "Voxelizer.h"
#include <iostream>
#include <fstream>
#include <bx/math.h>

namespace Engine::Renderer {

Voxelizer::Voxelizer() {}
Voxelizer::~Voxelizer() { shutdown(); }

void Voxelizer::init() {
    m_voxelTexture = bgfx::createTexture3D(VOXEL_RESOLUTION, VOXEL_RESOLUTION, VOXEL_RESOLUTION, false, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_COMPUTE_WRITE);

    auto loadMem = [](const std::string& path) -> const bgfx::Memory* {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            std::string fallback = std::string("../../../") + path;
            file.open(fallback, std::ios::binary | std::ios::ate);
        }
        if (!file.is_open()) return nullptr;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        const bgfx::Memory* mem = bgfx::alloc((uint32_t)size + 1);
        if (file.read((char*)mem->data, size)) {
            mem->data[size] = '\0';
            return mem;
        }
        return nullptr;
    };

    const bgfx::Memory* vsMem = loadMem("Engine/Renderer/Shaders/vs_voxelize.bin");
    const bgfx::Memory* fsMem = loadMem("Engine/Renderer/Shaders/fs_voxelize.bin");
    
    if (vsMem && fsMem) {
        bgfx::ShaderHandle vsh = bgfx::createShader(vsMem);
        bgfx::ShaderHandle fsh = bgfx::createShader(fsMem);
        m_voxelizationProgram = bgfx::createProgram(vsh, fsh, true);
    } else {
        std::cerr << "Failed to load voxelization shaders.\n";
    }
}

void Voxelizer::shutdown() {
    if (bgfx::isValid(m_voxelTexture)) {
        bgfx::destroy(m_voxelTexture);
        m_voxelTexture = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(m_voxelizationProgram)) {
        bgfx::destroy(m_voxelizationProgram);
        m_voxelizationProgram = BGFX_INVALID_HANDLE;
    }
}

void Voxelizer::voxelizeScene(const std::vector<RenderProxy>& proxies, bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh) {
    if (!bgfx::isValid(m_voxelizationProgram)) return;

    bgfx::setViewRect(View_Voxelize, 0, 0, VOXEL_RESOLUTION, VOXEL_RESOLUTION);
    
    float ortho[16];
    bx::mtxOrtho(ortho, -50.0f, 50.0f, -50.0f, 50.0f, -50.0f, 50.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    float view[16];
    bx::mtxIdentity(view);
    
    bgfx::setViewTransform(View_Voxelize, view, ortho);
    
    for (const auto& proxy : proxies) {
        bgfx::setTransform(proxy.worldTransform.m.data());
        
        // For MVP, we use the fallback cube if mesh is invalid. Since we don't have mesh access here easily, 
        // we assume caller passes the fallback. In a real system, proxy.mesh would be used.
        bgfx::setVertexBuffer(0, vbh);
        bgfx::setIndexBuffer(ibh);
        
        // Stage 1: image slot for UAV write
        bgfx::setImage(1, m_voxelTexture, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA8);
        
        // Disable face culling so we get backfaces too
        bgfx::setState(0); // State 0 means no depth test, no culling, no blending
        bgfx::submit(View_Voxelize, m_voxelizationProgram);
    }
}

} // namespace Engine::Renderer
