#pragma once
#include "SceneGraph/RenderScene.h"
#include "Camera.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "../Assets/AssetDatabase.h"

namespace Engine::Renderer {

struct MeshData {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t numIndices = 0;
};

enum RenderView : bgfx::ViewId {
    View_ShadowPass = 0,
    View_MainColor = 1,
    View_BloomThreshold = 2,
    View_Tonemap = 30, // Any intermediate passes will use ViewIDs 3-29
};

class RendererSystem {
public:
    static RendererSystem& instance() {
        static RendererSystem s_instance;
        return s_instance;
    }

    void init();
    void shutdown();
    void renderFrame(const Camera& camera, int width, int height, bgfx::FrameBufferHandle fb = BGFX_INVALID_HANDLE);
    bgfx::TextureHandle getTexture(const std::string& path);
    MeshHandle getMeshHandle(const Engine::Assets::AssetGuid& guid);

    // Bone Uniforms access
    bgfx::UniformHandle getBoneUniform() const { return u_boneTransforms; }

private:
    RendererSystem() = default;

    // Hardcoded küp nesnesi için VBO/IBO
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_shadowProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skinnedShadowProgram = BGFX_INVALID_HANDLE;

    // Shadow Map
    bgfx::FrameBufferHandle m_shadowMapFB = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightMtx = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texShadow = BGFX_INVALID_HANDLE;

    // Post-Processing
    bgfx::FrameBufferHandle m_hdrFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_bloomFBs[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::FrameBufferHandle m_bloomBlurFBs[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    
    bgfx::ProgramHandle m_bloomThresholdProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_bloomBlurProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_tonemapProgram = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle s_texBloom = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_bloomParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_blurParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_tonemapParams = BGFX_INVALID_HANDLE;

    int m_width = 0;
    int m_height = 0;

    // Uniforms
    bgfx::UniformHandle u_albedoRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_metallicEmissive = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texNormal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texMetallic = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_textureFlags = BGFX_INVALID_HANDLE;

    bgfx::TextureHandle m_defaultAlbedo = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_defaultNormal = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_defaultMetallic = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_defaultRoughness = BGFX_INVALID_HANDLE;

    std::unordered_map<std::string, bgfx::TextureHandle> m_textureCache;
    
    // Mesh Management
    std::unordered_map<Engine::Assets::AssetGuid, MeshHandle> m_meshGuidToHandle;
    std::unordered_map<MeshHandle, MeshData> m_meshes;
    uint32_t m_nextMeshHandle = 1;

    bgfx::UniformHandle u_boneTransforms = BGFX_INVALID_HANDLE;
};

struct SkinnedVertex {
    float x, y, z;
    uint32_t abgr;
    float nx, ny, nz;
    float u, v;
    uint8_t boneIndices[4];
    float boneWeights[4];

    static void init() {
        ms_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Uint8, false)
            .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Float)
            .end();
    }
    static bgfx::VertexLayout ms_layout;
};

} // namespace Engine::Renderer
