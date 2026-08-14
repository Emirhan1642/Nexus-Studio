#pragma once
#include "SceneGraph/RenderScene.h"
#include "Camera.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "../Assets/AssetDatabase.h"
#include "GI/Voxelizer.h"

namespace Engine::Renderer {

struct MeshData {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibhLods[3] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::IndexBufferHandle ibhLines = BGFX_INVALID_HANDLE;
    uint32_t numIndicesLods[3] = { 0, 0, 0 };
    uint32_t numLineIndices = 0;
    int numLods = 0;
};

enum RenderView : bgfx::ViewId {
    View_ShadowCascade0 = 0,
    View_ShadowCascade1 = 1,
    View_ShadowCascade2 = 2,
    View_MainColor = 3,
    View_SSGI = 4,
    View_BloomThreshold = 5,
    View_Tonemap = 30, // Any intermediate passes will use ViewIDs 6-29
    View_FXAA = 31,
    View_DOF = 32,
    View_MotionBlur = 33,
    View_SSR = 34,
    View_TAA = 35,
};

enum class ShadingMode {
    Face,       // Standard full shaded (Lit / PBR)
    Wireframe,  // Full wireframe lines
    Vertex      // Points / Vertices
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
    MeshHandle createDeformedCubeMesh(const std::vector<Engine::Math::Vector3>& localCorners);
    void updateDeformedCubeMesh(MeshHandle handle, const std::vector<Engine::Math::Vector3>& localCorners);
    void destroyMesh(MeshHandle handle);

    // Shading Mode (Face, Wireframe, Vertex)
    void setShadingMode(ShadingMode mode) { m_shadingMode = mode; }
    ShadingMode getShadingMode() const { return m_shadingMode; }

    // Bone Uniforms access
    bgfx::UniformHandle getBoneUniform() const { return u_boneTransforms; }
    
    // MVP Material Editor Override
    void setOverrideMaterial(bgfx::ProgramHandle handle) { m_overrideMaterial = handle; }

private:
    RendererSystem() = default;

    ShadingMode m_shadingMode = ShadingMode::Face;

    // Hardcoded küp nesnesi için VBO/IBO
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_cubeLineIbh = BGFX_INVALID_HANDLE;

    // Fullscreen quad için VBO/IBO (post-processing geçişleri)
    bgfx::VertexBufferHandle m_fsqVbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_fsqIbh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_shadowProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_skinnedShadowProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_overrideMaterial = BGFX_INVALID_HANDLE;

    // Shadow Map (CSM)
    bgfx::FrameBufferHandle m_shadowMapFBs[3] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::UniformHandle u_lightMtx = BGFX_INVALID_HANDLE; // Array of 3 matrices
    bgfx::UniformHandle s_texShadow0 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texShadow1 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texShadow2 = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_csmParams = BGFX_INVALID_HANDLE;

    // Post-Processing
    bgfx::FrameBufferHandle m_hdrFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_tonemapFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_bloomFBs[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::FrameBufferHandle m_bloomBlurFBs[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    bgfx::FrameBufferHandle m_dofFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_mbFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_ssrFB = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_taaFB[2] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
    int m_taaIndex = 0;
    int m_frameCount = 0;
    
    bgfx::ProgramHandle m_bloomThresholdProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_bloomBlurProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_dofProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_mbProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssrProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_taaProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_tonemapProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_ssgiProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_fxaaProgram = BGFX_INVALID_HANDLE;
    bgfx::FrameBufferHandle m_ssgiFB = BGFX_INVALID_HANDLE;

    bgfx::UniformHandle s_texBloom = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texNormalGBuffer = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texDepth = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texTonemap = BGFX_INVALID_HANDLE;
    
    bgfx::UniformHandle u_bloomParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_blurParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_tonemapParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ssgiParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_fxaaParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lodParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_dofParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_mbParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_prevViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_invViewProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_unjitteredInvProj = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_lightDir = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ssrParams = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_taaParams = BGFX_INVALID_HANDLE;

    int m_width = 0;
    int m_height = 0;

    // Uniforms
    bgfx::UniformHandle u_albedoRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_metallicEmissive = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texColor = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texNormal = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texMetallic = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texVoxel = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_texHistory = BGFX_INVALID_HANDLE;
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

    Engine::Math::Matrix4 m_prevViewProj;

    // VCT
    Voxelizer m_voxelizer;
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
