#include "Renderer.h"
#include <bx/timer.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <bx/allocator.h>
#include <bx/math.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>
#include <algorithm>

namespace Engine::Renderer {

static void imageReleaseCb(void* _ptr, void* _userData) {
    bimg::ImageContainer* imageContainer = (bimg::ImageContainer*)_userData;
    bimg::imageFree(imageContainer);
}

static float getHaltonSequence(int index, int base) {
    float f = 1.0f;
    float r = 0.0f;
    int i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

static bgfx::TextureHandle loadTextureFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return BGFX_INVALID_HANDLE;
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return BGFX_INVALID_HANDLE;

    static bx::DefaultAllocator allocator;
    
    bimg::ImageContainer* imageContainer = bimg::imageParse(&allocator, buffer.data(), static_cast<uint32_t>(size));
    if (!imageContainer) return BGFX_INVALID_HANDLE;

    const bgfx::Memory* mem = bgfx::makeRef(
        imageContainer->m_data,
        imageContainer->m_size,
        imageReleaseCb,
        imageContainer
    );

    return bgfx::createTexture2D(
        uint16_t(imageContainer->m_width),
        uint16_t(imageContainer->m_height),
        1 < imageContainer->m_numMips,
        imageContainer->m_numLayers,
        bgfx::TextureFormat::Enum(imageContainer->m_format),
        BGFX_TEXTURE_NONE | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
        mem
    );
}

bgfx::TextureHandle RendererSystem::getTexture(const std::string& path) {
    if (path.empty()) return BGFX_INVALID_HANDLE;
    auto it = m_textureCache.find(path);
    if (it != m_textureCache.end()) {
        return it->second;
    }
    bgfx::TextureHandle handle = loadTextureFromFile(path);
    m_textureCache[path] = handle;
    return handle;
}

struct PosColorTexCoordVertex {
    float x, y, z;
    uint32_t abgr;
    float nx, ny, nz; // Normal
    float u, v;       // TexCoord

    static void init() {
        ms_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }
    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorTexCoordVertex::ms_layout;
bgfx::VertexLayout SkinnedVertex::ms_layout;

static const PosColorTexCoordVertex s_cubeVertices[] = {
    // Front face (Z = 1)
    { -1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f,   0.0f, 1.0f }, // 0: Top-Left
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f,   1.0f, 1.0f }, // 1: Top-Right
    { -1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f,   0.0f, 0.0f }, // 2: Bottom-Left
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f,   1.0f, 0.0f }, // 3: Bottom-Right
    // Back face (Z = -1)
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f,   0.0f, 1.0f }, // 4: Top-Left
    { -1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f,   1.0f, 1.0f }, // 5: Top-Right
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f,   0.0f, 0.0f }, // 6: Bottom-Left
    { -1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f,   1.0f, 0.0f }, // 7: Bottom-Right
    // Top face (Y = 1)
    { -1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f,   0.0f, 1.0f }, // 8: Top-Left
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f,   1.0f, 1.0f }, // 9: Top-Right
    { -1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f,   0.0f, 0.0f }, // 10: Bottom-Left
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f,   1.0f, 0.0f }, // 11: Bottom-Right
    // Bottom face (Y = -1)
    { -1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f,   0.0f, 1.0f }, // 12: Top-Left
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f,   1.0f, 1.0f }, // 13: Top-Right
    { -1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f,   0.0f, 0.0f }, // 14: Bottom-Left
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f,   1.0f, 0.0f }, // 15: Bottom-Right
    // Right face (X = 1)
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f,   0.0f, 1.0f }, // 16: Top-Left
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f,   1.0f, 1.0f }, // 17: Top-Right
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f,   0.0f, 0.0f }, // 18: Bottom-Left
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f,   1.0f, 0.0f }, // 19: Bottom-Right
    // Left face (X = -1)
    { -1.0f,  1.0f, -1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f,   0.0f, 1.0f }, // 20: Top-Left
    { -1.0f,  1.0f,  1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f,   1.0f, 1.0f }, // 21: Top-Right
    { -1.0f, -1.0f, -1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f,   0.0f, 0.0f }, // 22: Bottom-Left
    { -1.0f, -1.0f,  1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f,   1.0f, 0.0f }, // 23: Bottom-Right
};

static const uint16_t s_cubeIndices[] = {
    0, 2, 1, 1, 2, 3, // Front
    4, 6, 5, 5, 6, 7, // Back
    8, 10, 9, 9, 10, 11, // Top
    12, 14, 13, 13, 14, 15, // Bottom
    16, 18, 17, 17, 18, 19, // Right
    20, 22, 21, 21, 22, 23, // Left
};


bgfx::ShaderHandle loadShader(const char* filepath) {
    std::ifstream file(filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        // Try fallback for running from build/bin/Debug
        std::string fallback = std::string("../../../") + filepath;
        file.open(fallback, std::ios::ate | std::ios::binary);
    }
    if (!file.is_open()) {
        std::cerr << "Failed to open shader: " << filepath << std::endl;
        return BGFX_INVALID_HANDLE;
    }
    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);

    const bgfx::Memory* mem = bgfx::alloc((uint32_t)fileSize + 1);
    file.read((char*)mem->data, fileSize);
    mem->data[fileSize] = '\0';
    file.close();

    return bgfx::createShader(mem);
}

void RendererSystem::init() {
    PosColorTexCoordVertex::init();
    SkinnedVertex::init();

    m_vbh = bgfx::createVertexBuffer(
        bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)),
        PosColorTexCoordVertex::ms_layout
    );

    m_ibh = bgfx::createIndexBuffer(
        bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices))
    );

    // Generate line index buffer for wireframe cube (all 3 edges of every triangle)
    std::vector<uint16_t> cubeLineIndices;
    cubeLineIndices.reserve(sizeof(s_cubeIndices) / sizeof(s_cubeIndices[0]) * 2);
    for (size_t i = 0; i < sizeof(s_cubeIndices) / sizeof(s_cubeIndices[0]); i += 3) {
        uint16_t i0 = s_cubeIndices[i];
        uint16_t i1 = s_cubeIndices[i+1];
        uint16_t i2 = s_cubeIndices[i+2];
        cubeLineIndices.push_back(i0); cubeLineIndices.push_back(i1);
        cubeLineIndices.push_back(i1); cubeLineIndices.push_back(i2);
        cubeLineIndices.push_back(i2); cubeLineIndices.push_back(i0);
    }
    m_cubeLineIbh = bgfx::createIndexBuffer(
        bgfx::copy(cubeLineIndices.data(), static_cast<uint32_t>(cubeLineIndices.size() * sizeof(uint16_t)))
    );

    // Fullscreen quad: NDC koordinatlarında tam ekranı kaplayan 2 üçgen
    // Z=0, UV (0,0)->(1,1)
    static const PosColorTexCoordVertex s_fsqVertices[] = {
        // x,     y,     z,     abgr,        nx,   ny,   nz,   u,    v
        { -1.0f, -1.0f,  0.0f,  0xffffffff,  0.0f, 0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-Left
        {  1.0f, -1.0f,  0.0f,  0xffffffff,  0.0f, 0.0f, 1.0f, 1.0f, 1.0f }, // Bottom-Right
        {  1.0f,  1.0f,  0.0f,  0xffffffff,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f }, // Top-Right
        { -1.0f,  1.0f,  0.0f,  0xffffffff,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f }, // Top-Left
    };
    static const uint16_t s_fsqIndices[] = { 0, 1, 2,  0, 2, 3 };

    m_fsqVbh = bgfx::createVertexBuffer(
        bgfx::makeRef(s_fsqVertices, sizeof(s_fsqVertices)),
        PosColorTexCoordVertex::ms_layout
    );
    m_fsqIbh = bgfx::createIndexBuffer(
        bgfx::makeRef(s_fsqIndices, sizeof(s_fsqIndices))
    );

    u_albedoRoughness = bgfx::createUniform("u_albedoRoughness", bgfx::UniformType::Vec4);
    u_metallicEmissive = bgfx::createUniform("u_metallicEmissive", bgfx::UniformType::Vec4);
    u_textureFlags = bgfx::createUniform("u_textureFlags", bgfx::UniformType::Vec4);
    
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texNormal = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);
    s_texMetallic = bgfx::createUniform("s_texMetallic", bgfx::UniformType::Sampler);
    s_texRoughness = bgfx::createUniform("s_texRoughness", bgfx::UniformType::Sampler);
    s_texBloom = bgfx::createUniform("s_texBloom", bgfx::UniformType::Sampler);
    s_texNormalGBuffer = bgfx::createUniform("s_texNormalGBuffer", bgfx::UniformType::Sampler);
    s_texDepth = bgfx::createUniform("s_texDepth", bgfx::UniformType::Sampler);
    s_texVoxel = bgfx::createUniform("s_texVoxel", bgfx::UniformType::Sampler);
    s_texTonemap = bgfx::createUniform("s_texTonemap", bgfx::UniformType::Sampler);
    s_texHistory = bgfx::createUniform("s_texHistory", bgfx::UniformType::Sampler);
    
    u_bloomParams = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    u_blurParams = bgfx::createUniform("u_blurParams", bgfx::UniformType::Vec4);
    u_tonemapParams = bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4);
    u_ssgiParams = bgfx::createUniform("u_ssgiParams", bgfx::UniformType::Vec4);
    u_fxaaParams = bgfx::createUniform("u_fxaaParams", bgfx::UniformType::Vec4);
    u_lodParams = bgfx::createUniform("u_lodParams", bgfx::UniformType::Vec4);
    u_dofParams = bgfx::createUniform("u_dofParams", bgfx::UniformType::Vec4);
    u_mbParams = bgfx::createUniform("u_mbParams", bgfx::UniformType::Vec4);
    u_prevViewProj = bgfx::createUniform("u_prevViewProj", bgfx::UniformType::Mat4);
    u_lightDir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
    u_ssrParams = bgfx::createUniform("u_ssrParams", bgfx::UniformType::Vec4);
    u_taaParams = bgfx::createUniform("u_taaParams", bgfx::UniformType::Vec4);
    u_unjitteredInvProj = bgfx::createUniform("u_unjitteredInvProj", bgfx::UniformType::Mat4);
    
    m_prevViewProj = Engine::Math::Matrix4::identity();
    
    // Create Bone Transforms uniform (Array of 64 Mat4)
    u_boneTransforms = bgfx::createUniform("u_boneTransforms", bgfx::UniformType::Mat4, 64);
    
    // Shadow Map Setup (CSM)
    u_lightMtx = bgfx::createUniform("u_lightMtx", bgfx::UniformType::Mat4, 3);
    s_texShadow0 = bgfx::createUniform("s_texShadow0", bgfx::UniformType::Sampler);
    s_texShadow1 = bgfx::createUniform("s_texShadow1", bgfx::UniformType::Sampler);
    s_texShadow2 = bgfx::createUniform("s_texShadow2", bgfx::UniformType::Sampler);
    u_csmParams = bgfx::createUniform("u_csmParams", bgfx::UniformType::Vec4);
    
    // Create shadow map textures (D16 is widely supported for shadow maps)
    for (int i = 0; i < 3; ++i) {
        bgfx::TextureHandle shadowMapTexture = bgfx::createTexture2D(
            2048, 2048, false, 1, bgfx::TextureFormat::D16,
            BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
        );
        m_shadowMapFBs[i] = bgfx::createFrameBuffer(1, &shadowMapTexture, true);
    }

    const bgfx::Memory* memAlbedo = bgfx::copy("\xff\xff\xff\xff", 4);
    m_defaultAlbedo = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, memAlbedo);

    const bgfx::Memory* memNormal = bgfx::copy("\x80\x80\xff\xff", 4); // (0.5, 0.5, 1.0, 1.0)
    m_defaultNormal = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, memNormal);

    const bgfx::Memory* memMetallic = bgfx::copy("\x00\x00\x00\xff", 4);
    m_defaultMetallic = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, memMetallic);

    const bgfx::Memory* memRoughness = bgfx::copy("\x00\x00\x00\xff", 4);
    m_defaultRoughness = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, memRoughness);

    // Try to load shaders
    bgfx::ShaderHandle vsh = loadShader("Engine/Renderer/Shaders/vs_pbr.bin");
    bgfx::ShaderHandle fsh = loadShader("Engine/Renderer/Shaders/fs_pbr.bin");
    if (bgfx::isValid(vsh) && bgfx::isValid(fsh)) {
        m_program = bgfx::createProgram(vsh, fsh, true);
    }
    
    bgfx::ShaderHandle shadowVsh = loadShader("Engine/Renderer/Shaders/vs_shadow.bin");
    bgfx::ShaderHandle shadowFsh = loadShader("Engine/Renderer/Shaders/fs_shadow.bin");
    if (bgfx::isValid(shadowVsh) && bgfx::isValid(shadowFsh)) {
        m_shadowProgram = bgfx::createProgram(shadowVsh, shadowFsh, true);
    }
    
    bgfx::ShaderHandle skinnedShadowVsh = loadShader("Engine/Renderer/Shaders/vs_skinned_shadow.bin");
    // We can reuse fs_shadow for skinned shadow as well
    bgfx::ShaderHandle skinnedShadowFsh = loadShader("Engine/Renderer/Shaders/fs_shadow.bin");
    if (bgfx::isValid(skinnedShadowVsh) && bgfx::isValid(skinnedShadowFsh)) {
        m_skinnedShadowProgram = bgfx::createProgram(skinnedShadowVsh, skinnedShadowFsh, true);
    }

    bgfx::ShaderHandle vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsBloomThreshold = loadShader("Engine/Renderer/Shaders/fs_bloom_threshold.bin");
    bgfx::ShaderHandle fsBloomBlur = loadShader("Engine/Renderer/Shaders/fs_bloom_blur.bin");
    bgfx::ShaderHandle fsTonemap = loadShader("Engine/Renderer/Shaders/fs_tonemap.bin");
    bgfx::ShaderHandle fsSsgi = loadShader("Engine/Renderer/Shaders/fs_ssgi.bin");
    
    if (bgfx::isValid(vsFullscreen)) {
        if (bgfx::isValid(fsBloomThreshold)) m_bloomThresholdProgram = bgfx::createProgram(vsFullscreen, fsBloomThreshold, true);
        else bgfx::destroy(vsFullscreen); // Will recreate below if needed, wait bgfx::createProgram destroys them if true is passed.
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsBloomBlur)) {
        m_bloomBlurProgram = bgfx::createProgram(vsFullscreen, fsBloomBlur, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsSsr = loadShader("Engine/Renderer/Shaders/fs_ssr.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsSsr)) {
        m_ssrProgram = bgfx::createProgram(vsFullscreen, fsSsr, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsTaa = loadShader("Engine/Renderer/Shaders/fs_taa.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsTaa)) {
        m_taaProgram = bgfx::createProgram(vsFullscreen, fsTaa, true);
    }

    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsTonemap)) {
        m_tonemapProgram = bgfx::createProgram(vsFullscreen, fsTonemap, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsSsgi)) {
        m_ssgiProgram = bgfx::createProgram(vsFullscreen, fsSsgi, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsFxaa = loadShader("Engine/Renderer/Shaders/fs_fxaa.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsFxaa)) {
        m_fxaaProgram = bgfx::createProgram(vsFullscreen, fsFxaa, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsDof = loadShader("Engine/Renderer/Shaders/fs_dof.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsDof)) {
        m_dofProgram = bgfx::createProgram(vsFullscreen, fsDof, true);
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    bgfx::ShaderHandle fsMb = loadShader("Engine/Renderer/Shaders/fs_mb.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsMb)) {
        m_mbProgram = bgfx::createProgram(vsFullscreen, fsMb, true);
    }

    bgfx::setViewClear(View_MainColor, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewClear(View_ShadowCascade0, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewClear(View_ShadowCascade1, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewClear(View_ShadowCascade2, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    m_voxelizer.init();
}

void RendererSystem::shutdown() {
    m_voxelizer.shutdown();

    bgfx::destroy(m_ibh);
    if (bgfx::isValid(m_cubeLineIbh)) bgfx::destroy(m_cubeLineIbh);
    bgfx::destroy(m_vbh);
    if (bgfx::isValid(m_fsqIbh)) bgfx::destroy(m_fsqIbh);
    if (bgfx::isValid(m_fsqVbh)) bgfx::destroy(m_fsqVbh);
    bgfx::destroy(m_program);
    if (bgfx::isValid(m_shadowProgram)) bgfx::destroy(m_shadowProgram);
    if (bgfx::isValid(m_skinnedShadowProgram)) bgfx::destroy(m_skinnedShadowProgram);
    for (int i = 0; i < 3; ++i) {
        if (bgfx::isValid(m_shadowMapFBs[i])) bgfx::destroy(m_shadowMapFBs[i]);
    }
    if (bgfx::isValid(u_lightMtx)) bgfx::destroy(u_lightMtx);
    if (bgfx::isValid(u_csmParams)) bgfx::destroy(u_csmParams);
    if (bgfx::isValid(s_texShadow0)) bgfx::destroy(s_texShadow0);
    if (bgfx::isValid(s_texShadow1)) bgfx::destroy(s_texShadow1);
    if (bgfx::isValid(s_texShadow2)) bgfx::destroy(s_texShadow2);
    
    bgfx::destroy(u_albedoRoughness);
    bgfx::destroy(u_metallicEmissive);
    bgfx::destroy(u_textureFlags);
    
    bgfx::destroy(s_texColor);
    bgfx::destroy(s_texNormal);
    bgfx::destroy(s_texMetallic);
    bgfx::destroy(s_texRoughness);
    bgfx::destroy(s_texBloom);
    bgfx::destroy(s_texNormalGBuffer);
    bgfx::destroy(s_texDepth);
    bgfx::destroy(s_texTonemap);
    bgfx::destroy(s_texHistory);
    
    bgfx::destroy(u_bloomParams);
    bgfx::destroy(u_blurParams);
    bgfx::destroy(u_tonemapParams);
    bgfx::destroy(u_ssgiParams);
    bgfx::destroy(u_fxaaParams);
    bgfx::destroy(u_lodParams);
    bgfx::destroy(u_dofParams);
    bgfx::destroy(u_mbParams);
    bgfx::destroy(u_prevViewProj);
    bgfx::destroy(u_lightDir);
    bgfx::destroy(u_ssrParams);
    bgfx::destroy(u_taaParams);
    bgfx::destroy(u_unjitteredInvProj);
    
    if (bgfx::isValid(m_bloomThresholdProgram)) bgfx::destroy(m_bloomThresholdProgram);
    if (bgfx::isValid(m_bloomBlurProgram)) bgfx::destroy(m_bloomBlurProgram);
    if (bgfx::isValid(m_tonemapProgram)) bgfx::destroy(m_tonemapProgram);
    if (bgfx::isValid(m_ssgiProgram)) bgfx::destroy(m_ssgiProgram);
    if (bgfx::isValid(m_bloomBlurProgram)) bgfx::destroy(m_bloomBlurProgram);
    if (bgfx::isValid(m_dofProgram)) bgfx::destroy(m_dofProgram);
    if (bgfx::isValid(m_mbProgram)) bgfx::destroy(m_mbProgram);
    if (bgfx::isValid(m_ssrProgram)) bgfx::destroy(m_ssrProgram);
    if (bgfx::isValid(m_taaProgram)) bgfx::destroy(m_taaProgram);
    
    if (bgfx::isValid(m_hdrFB)) bgfx::destroy(m_hdrFB);
    if (bgfx::isValid(m_tonemapFB)) bgfx::destroy(m_tonemapFB);
    if (bgfx::isValid(m_ssgiFB)) bgfx::destroy(m_ssgiFB);
    if (bgfx::isValid(m_ssrFB)) bgfx::destroy(m_ssrFB);
    if (bgfx::isValid(m_dofFB)) bgfx::destroy(m_dofFB);
    if (bgfx::isValid(m_mbFB)) bgfx::destroy(m_mbFB);
    if (bgfx::isValid(m_taaFB[0])) bgfx::destroy(m_taaFB[0]);
    if (bgfx::isValid(m_taaFB[1])) bgfx::destroy(m_taaFB[1]);
    for (int i = 0; i < 5; ++i) {
        if (bgfx::isValid(m_bloomFBs[i])) bgfx::destroy(m_bloomFBs[i]);
        if (bgfx::isValid(m_bloomBlurFBs[i])) bgfx::destroy(m_bloomBlurFBs[i]);
    }
    
    if (bgfx::isValid(s_texNormalGBuffer)) {
        bgfx::destroy(s_texNormalGBuffer);
        s_texNormalGBuffer = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_texDepth)) {
        bgfx::destroy(s_texDepth);
        s_texDepth = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(s_texVoxel)) {
        bgfx::destroy(s_texVoxel);
        s_texVoxel = BGFX_INVALID_HANDLE;
    }
    m_textureCache.clear();
    
    for (auto& [handle, meshData] : m_meshes) {
        if (bgfx::isValid(meshData.vbh)) bgfx::destroy(meshData.vbh);
        for (int i = 0; i < meshData.numLods; ++i) {
            if (bgfx::isValid(meshData.ibhLods[i])) bgfx::destroy(meshData.ibhLods[i]);
        }
        if (bgfx::isValid(meshData.ibhLines)) bgfx::destroy(meshData.ibhLines);
    }
    m_meshes.clear();
    m_meshGuidToHandle.clear();

    if (bgfx::isValid(m_defaultAlbedo)) bgfx::destroy(m_defaultAlbedo);
    if (bgfx::isValid(m_defaultNormal)) bgfx::destroy(m_defaultNormal);
    if (bgfx::isValid(m_defaultMetallic)) bgfx::destroy(m_defaultMetallic);
    if (bgfx::isValid(m_defaultRoughness)) bgfx::destroy(m_defaultRoughness);
}

void RendererSystem::renderFrame(const Camera& camera, int width, int height, bgfx::FrameBufferHandle fb) {
    const auto& proxies = RenderScene::instance().getProxies();
    
    // 1. Voxelize Scene
    m_voxelizer.voxelizeScene(proxies, m_vbh, m_ibh);

    // -----------------------------------------
    // PASS 1: CASCADED SHADOW MAP PASS
    // -----------------------------------------
    const int shadowMapSize = 2048;
    // Directional Light Settings (Hardcoded for MVP)
    Engine::Math::Vector3 lightDir = {0.577f, 0.577f, -0.577f}; // Normalized {1, 1, -1}
    
    float cascadeDistances[4] = { 1.0f, 15.0f, 50.0f, 150.0f }; // Near, C0, C1, C2
    float csmParams[4] = { cascadeDistances[1], cascadeDistances[2], cascadeDistances[3], 0.0f };
    bgfx::setUniform(u_csmParams, csmParams);

    float lightVPs[48]; // 3 * 16

    for (int cascadeIdx = 0; cascadeIdx < 3; ++cascadeIdx) {
        bgfx::ViewId viewId = View_ShadowCascade0 + cascadeIdx;
        bgfx::setViewRect(viewId, 0, 0, shadowMapSize, shadowMapSize);
        bgfx::setViewFrameBuffer(viewId, m_shadowMapFBs[cascadeIdx]);
        bgfx::setViewClear(viewId, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        float dist = cascadeDistances[cascadeIdx + 1];
        float boxSize = dist * 1.5f;
        
        Engine::Math::Vector3 lightPos = { 
            camera.position.x + lightDir.x * boxSize, 
            camera.position.y + lightDir.y * boxSize, 
            camera.position.z + lightDir.z * boxSize 
        };
        
        float lightView[16];
        bx::mtxLookAt(lightView, 
            {lightPos.x, lightPos.y, lightPos.z},
            {camera.position.x, camera.position.y, camera.position.z},
            {0.0f, 1.0f, 0.0f}
        );

        float lightProj[16];
        bx::mtxOrtho(lightProj, -boxSize, boxSize, -boxSize, boxSize, 1.0f, boxSize * 2.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(viewId, lightView, lightProj);

        float lightVP[16];
        bx::mtxMul(lightVP, lightView, lightProj);
        memcpy(&lightVPs[cascadeIdx * 16], lightVP, sizeof(float) * 16);

        bgfx::touch(viewId);
    }

    bgfx::setUniform(u_lightMtx, lightVPs, 3);

    RenderScene::instance().flushDirtyTransforms();

    for (const auto& proxy : proxies) {
        if (!proxy.visible) continue;
        
        float distSq = (proxy.boundsCenter.x - camera.position.x) * (proxy.boundsCenter.x - camera.position.x) +
                       (proxy.boundsCenter.y - camera.position.y) * (proxy.boundsCenter.y - camera.position.y) +
                       (proxy.boundsCenter.z - camera.position.z) * (proxy.boundsCenter.z - camera.position.z);
        float dist = std::sqrt(distSq);

        int lodIndex0 = 0;
        int lodIndex1 = -1;
        float fade = 1.0f; // 1.0 means fully lodIndex0
        float transitionRange = 5.0f;

        if (dist > 100.0f) {
            lodIndex0 = 2;
        } else if (dist > 100.0f - transitionRange) {
            lodIndex0 = 2; lodIndex1 = 1;
            fade = (dist - (100.0f - transitionRange)) / transitionRange;
        } else if (dist > 50.0f) {
            lodIndex0 = 1;
        } else if (dist > 50.0f - transitionRange) {
            lodIndex0 = 1; lodIndex1 = 0;
            fade = (dist - (50.0f - transitionRange)) / transitionRange;
        }

        auto submitShadowMesh = [&](int lod, float lodFade, int cascadeIdx, bgfx::ProgramHandle shadowProg) {
            bgfx::setTransform(proxy.worldTransform.m.data());
            if (proxy.mesh != InvalidHandle) {
                auto it = m_meshes.find(proxy.mesh);
                if (it != m_meshes.end()) {
                    bgfx::setVertexBuffer(0, it->second.vbh);
                    int safeLod = lod;
                    if (safeLod >= it->second.numLods) safeLod = it->second.numLods - 1;
                    if (safeLod < 0) safeLod = 0;
                    bgfx::setIndexBuffer(it->second.ibhLods[safeLod]);
                } else {
                    bgfx::setVertexBuffer(0, m_vbh);
                    bgfx::setIndexBuffer(m_ibh);
                }
            } else {
                bgfx::setVertexBuffer(0, m_vbh);
                bgfx::setIndexBuffer(m_ibh);
            }
            if (!proxy.boneTransforms.empty()) {
                uint16_t numBones = static_cast<uint16_t>(std::min(proxy.boneTransforms.size(), size_t(64)));
                bgfx::setUniform(u_boneTransforms, proxy.boneTransforms.data(), numBones);
            }
            float lodParams[4] = { lodFade, 0.0f, 0.0f, 0.0f };
            bgfx::setUniform(u_lodParams, lodParams);
            uint64_t state = (BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK) | BGFX_STATE_CULL_CCW | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z;
            bgfx::setState(state);
            bgfx::submit(View_ShadowCascade0 + cascadeIdx, shadowProg);
        };

        bgfx::ProgramHandle shadowProg = proxy.boneTransforms.empty() ? m_shadowProgram : m_skinnedShadowProgram;
        if (bgfx::isValid(shadowProg)) {
            for (int cascadeIdx = 0; cascadeIdx < 3; ++cascadeIdx) {
                submitShadowMesh(lodIndex0, fade, cascadeIdx, shadowProg);
                if (lodIndex1 != -1) {
                    submitShadowMesh(lodIndex1, 1.0f - fade, cascadeIdx, shadowProg);
                }
            }
        }
    }

    // -----------------------------------------
    // FRAMEBUFFER RESIZE HANDLING
    // -----------------------------------------
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;

        if (bgfx::isValid(m_hdrFB)) bgfx::destroy(m_hdrFB);
        if (bgfx::isValid(m_tonemapFB)) bgfx::destroy(m_tonemapFB);
        if (bgfx::isValid(m_ssgiFB)) bgfx::destroy(m_ssgiFB);
        if (bgfx::isValid(m_ssrFB)) bgfx::destroy(m_ssrFB);
        if (bgfx::isValid(m_dofFB)) bgfx::destroy(m_dofFB);
        if (bgfx::isValid(m_mbFB)) bgfx::destroy(m_mbFB);
        if (bgfx::isValid(m_taaFB[0])) bgfx::destroy(m_taaFB[0]);
        if (bgfx::isValid(m_taaFB[1])) bgfx::destroy(m_taaFB[1]);
        for (int i = 0; i < 5; ++i) {
            if (bgfx::isValid(m_bloomFBs[i])) bgfx::destroy(m_bloomFBs[i]);
            if (bgfx::isValid(m_bloomBlurFBs[i])) bgfx::destroy(m_bloomBlurFBs[i]);
        }

        bgfx::TextureHandle hdrColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle hdrNormalTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle hdrDepthTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle hdrTextures[] = { hdrColorTex, hdrNormalTex, hdrDepthTex };
        m_hdrFB = bgfx::createFrameBuffer(3, hdrTextures, true);

        bgfx::TextureHandle tonemapColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_tonemapFB = bgfx::createFrameBuffer(1, &tonemapColorTex, true);

        bgfx::TextureHandle ssgiColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_ssgiFB = bgfx::createFrameBuffer(1, &ssgiColorTex, true);
        
        bgfx::TextureHandle ssrColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_ssrFB = bgfx::createFrameBuffer(1, &ssrColorTex, true);
        
        bgfx::TextureHandle dofColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_dofFB = bgfx::createFrameBuffer(1, &dofColorTex, true);

        bgfx::TextureHandle mbColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_mbFB = bgfx::createFrameBuffer(1, &mbColorTex, true);

        bgfx::TextureHandle taaColorTex0 = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_taaFB[0] = bgfx::createFrameBuffer(1, &taaColorTex0, true);
        
        bgfx::TextureHandle taaColorTex1 = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        m_taaFB[1] = bgfx::createFrameBuffer(1, &taaColorTex1, true);

        int bw = width;
        int bh = height;
        for (int i = 0; i < 5; ++i) {
            bw = bx::max(1, bw / 2);
            bh = bx::max(1, bh / 2);
            bgfx::TextureHandle bTex = bgfx::createTexture2D(bw, bh, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
            m_bloomFBs[i] = bgfx::createFrameBuffer(1, &bTex, true);
            bgfx::TextureHandle bbTex = bgfx::createTexture2D(bw, bh, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
            m_bloomBlurFBs[i] = bgfx::createFrameBuffer(1, &bbTex, true);
        }
    }

    // -----------------------------------------
    // PASS 2: MAIN COLOR PASS (Render to HDR FB)
    // -----------------------------------------
    bgfx::setViewRect(View_MainColor, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewFrameBuffer(View_MainColor, m_hdrFB);
    bgfx::setViewClear(View_MainColor, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    m_frameCount++;

    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 unjitteredProj = camera.getProjectionMatrix(float(width) / float(height));
    Engine::Math::Matrix4 proj = unjitteredProj;
    // Note: TAA projection jitter is skipped while TAA resolve pass is inactive to prevent subpixel flicker.

    bgfx::setViewTransform(View_MainColor, view.m.data(), proj.m.data());

    bgfx::touch(View_MainColor);

    bgfx::TextureHandle shadowMapTextures[3] = {
        bgfx::getTexture(m_shadowMapFBs[0], 0),
        bgfx::getTexture(m_shadowMapFBs[1], 0),
        bgfx::getTexture(m_shadowMapFBs[2], 0)
    };

    for (const auto& proxy : proxies) {
        if (!proxy.visible) continue;
        if (m_shadingMode == ShadingMode::Vertex || m_shadingMode == ShadingMode::Edge) continue; // In Vertex & Edge Modes, faces & diagonals are hidden (only clean quad cage edges rendered)
        
        float distSq = (proxy.boundsCenter.x - camera.position.x) * (proxy.boundsCenter.x - camera.position.x) +
                       (proxy.boundsCenter.y - camera.position.y) * (proxy.boundsCenter.y - camera.position.y) +
                       (proxy.boundsCenter.z - camera.position.z) * (proxy.boundsCenter.z - camera.position.z);
        float dist = std::sqrt(distSq);

        int lodIndex0 = 0;
        int lodIndex1 = -1;
        float fade = 1.0f; // 1.0 means fully lodIndex0
        float transitionRange = 5.0f;

        if (dist > 100.0f) {
            lodIndex0 = 2;
        } else if (dist > 100.0f - transitionRange) {
            lodIndex0 = 2; lodIndex1 = 1;
            fade = (dist - (100.0f - transitionRange)) / transitionRange;
        } else if (dist > 50.0f) {
            lodIndex0 = 1;
        } else if (dist > 50.0f - transitionRange) {
            lodIndex0 = 1; lodIndex1 = 0;
            fade = (dist - (50.0f - transitionRange)) / transitionRange;
        }
        
        auto submitColorMesh = [&](int lod, float lodFade) {
            bgfx::setTransform(proxy.worldTransform.m.data());
            
            bool isEdge = (m_shadingMode == ShadingMode::Edge);

            if (proxy.mesh != InvalidHandle) {
                auto it = m_meshes.find(proxy.mesh);
                if (it != m_meshes.end()) {
                    bgfx::setVertexBuffer(0, it->second.vbh);
                    if (isEdge && bgfx::isValid(it->second.ibhLines)) {
                        bgfx::setIndexBuffer(it->second.ibhLines);
                    } else {
                        int safeLod = lod;
                        if (safeLod >= it->second.numLods) safeLod = it->second.numLods - 1;
                        if (safeLod < 0) safeLod = 0;
                        bgfx::setIndexBuffer(it->second.ibhLods[safeLod]);
                    }
                } else {
                    bgfx::setVertexBuffer(0, m_vbh);
                    bgfx::setIndexBuffer(isEdge && bgfx::isValid(m_cubeLineIbh) ? m_cubeLineIbh : m_ibh);
                }
            } else {
                bgfx::setVertexBuffer(0, m_vbh);
                bgfx::setIndexBuffer(isEdge && bgfx::isValid(m_cubeLineIbh) ? m_cubeLineIbh : m_ibh);
            }

            if (!proxy.boneTransforms.empty()) {
                uint16_t numBones = static_cast<uint16_t>(std::min(proxy.boneTransforms.size(), size_t(64)));
                bgfx::setUniform(u_boneTransforms, proxy.boneTransforms.data(), numBones);
            }

            float albedoRoughness[4];
            if (isEdge) {
                // Boost edge wireframe brightness to ensure high contrast against dark background regardless of normal/lighting
                albedoRoughness[0] = proxy.material.albedo.x * 3.5f + 0.5f;
                albedoRoughness[1] = proxy.material.albedo.y * 3.5f + 0.5f;
                albedoRoughness[2] = proxy.material.albedo.z * 3.5f + 0.5f;
                albedoRoughness[3] = 0.0f;
            } else {
                albedoRoughness[0] = proxy.material.albedo.x;
                albedoRoughness[1] = proxy.material.albedo.y;
                albedoRoughness[2] = proxy.material.albedo.z;
                albedoRoughness[3] = proxy.material.roughness;
            }

            float metallicEmissive[4] = { proxy.material.metallic, proxy.material.emissiveStrength, 0.0f, 0.0f };
            float textureFlags[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            float lodParams[4] = { lodFade, 0.0f, 0.0f, 0.0f };
            
            bgfx::setTexture(0, s_texColor, bgfx::isValid(proxy.material.albedoTexture) ? proxy.material.albedoTexture : m_defaultAlbedo);
            bgfx::setTexture(1, s_texNormal, bgfx::isValid(proxy.material.normalTexture) ? proxy.material.normalTexture : m_defaultNormal);
            bgfx::setTexture(2, s_texMetallic, bgfx::isValid(proxy.material.metallicTexture) ? proxy.material.metallicTexture : m_defaultMetallic);
            bgfx::setTexture(3, s_texRoughness, bgfx::isValid(proxy.material.roughnessTexture) ? proxy.material.roughnessTexture : m_defaultRoughness);
            
            bgfx::setTexture(4, s_texShadow0, shadowMapTextures[0]);
            bgfx::setTexture(5, s_texShadow1, shadowMapTextures[1]);
            bgfx::setTexture(6, s_texShadow2, shadowMapTextures[2]);
            if (bgfx::isValid(m_voxelizer.getVoxelTexture())) {
                bgfx::setTexture(7, s_texVoxel, m_voxelizer.getVoxelTexture());
            }

            bgfx::setUniform(u_albedoRoughness, albedoRoughness);
            bgfx::setUniform(u_metallicEmissive, metallicEmissive);
            bgfx::setUniform(u_textureFlags, textureFlags);
            bgfx::setUniform(u_lodParams, lodParams);

            uint64_t state = BGFX_STATE_DEFAULT;
            if (isEdge) {
                state = (BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK) | BGFX_STATE_PT_LINES | BGFX_STATE_LINEAA;
            }
            bgfx::setState(state);
            
            bgfx::ProgramHandle activeProgram = m_program;
            if (bgfx::isValid(proxy.material.customShader)) {
                activeProgram = proxy.material.customShader;
            } else if (bgfx::isValid(m_overrideMaterial)) {
                activeProgram = m_overrideMaterial;
            }
            bgfx::submit(View_MainColor, activeProgram);
        };

        submitColorMesh(lodIndex0, fade);
        if (lodIndex1 != -1) {
            submitColorMesh(lodIndex1, 1.0f - fade);
        }
    }
    
    // -----------------------------------------
    // PASS 3: POST-PROCESSING
    // HDR Color -> Tonemap -> FXAA (backbuffer)
    // NOT: SSGI/SSR/DoF/MB/TAA gecici olarak devre disi -- bu passlar
    // u_invProj gibi set edilmemis uniformlar kullanip sahneyi bozuyor.
    // -----------------------------------------
    bgfx::TextureHandle hdrColorTex = bgfx::getTexture(m_hdrFB, 0);

    // 3.1 Tonemap: HDR rengi LDR'ye donustur
    float tonemapParams[4] = { 1.0f /*exposure*/, 0.0f /*bloom int - disabled*/, 0.0f, 0.0f };
    bgfx::setUniform(u_tonemapParams, tonemapParams);
    bgfx::setTexture(0, s_texColor, hdrColorTex);

    // Bloom devre disi: 1x1 siyah texture
    bgfx::setTexture(1, s_texBloom, m_defaultMetallic); // 1x1 siyah texture (metallic default)

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setViewRect(View_Tonemap, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewFrameBuffer(View_Tonemap, m_tonemapFB);
    bgfx::setViewClear(View_Tonemap, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    bgfx::setVertexBuffer(0, m_fsqVbh);
    bgfx::setIndexBuffer(m_fsqIbh);
    bgfx::submit(View_Tonemap, m_tonemapProgram);

    // 3.2 FXAA: Tonemap ciktisini ekrana yaz
    if (bgfx::isValid(m_fxaaProgram)) {
        float fxaaParams[4] = { 1.0f / width, 1.0f / height, 0.0f, 0.0f };
        bgfx::setUniform(u_fxaaParams, fxaaParams);
        bgfx::setTexture(0, s_texTonemap, bgfx::getTexture(m_tonemapFB, 0));

        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setViewRect(View_FXAA, 0, 0, uint16_t(width), uint16_t(height));
        bgfx::setViewFrameBuffer(View_FXAA, fb);
        bgfx::setViewClear(View_FXAA, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
        bgfx::setVertexBuffer(0, m_fsqVbh);
        bgfx::setIndexBuffer(m_fsqIbh);
        bgfx::submit(View_FXAA, m_fxaaProgram);
    } else {

        // If FXAA shader is invalid, we don't have a simple copy shader yet.
        // It's safer to leave the backbuffer as is than to call bgfx::blit,
        // which crashes if the backbuffer doesn't have BGFX_TEXTURE_BLIT_DST.
    }

    
    // Save current frame's View-Projection matrix for next frame (motion blur - currently disabled)
    Engine::Math::Matrix4 viewProj = camera.getProjectionMatrix((float)width/(float)height) * camera.getViewMatrix();
    m_prevViewProj = viewProj;
}

MeshHandle RendererSystem::getMeshHandle(const Engine::Assets::AssetGuid& guid) {
    if (!guid.isValid()) return InvalidHandle;

    auto it = m_meshGuidToHandle.find(guid);
    if (it != m_meshGuidToHandle.end()) {
        return it->second;
    }

    // Try to load from AssetDatabase
    auto assetOpt = Engine::Assets::AssetDatabase::instance().find(guid);
    if (!assetOpt) return InvalidHandle;

    auto importedMesh = Engine::Assets::AssetDatabase::instance().getSkeletalMesh(guid);
    if (!importedMesh) return InvalidHandle;

    if (importedMesh->vertices.empty() || importedMesh->lodIndices.empty() || importedMesh->lodIndices[0].empty()) {
        return InvalidHandle;
    }

    // Convert to SkinnedVertex
    std::vector<SkinnedVertex> vertices(importedMesh->vertices.size());
    for (size_t i = 0; i < importedMesh->vertices.size(); ++i) {
        const auto& src = importedMesh->vertices[i];
        auto& dst = vertices[i];
        dst.x = src.x; dst.y = src.y; dst.z = src.z;
        dst.nx = src.nx; dst.ny = src.ny; dst.nz = src.nz;
        dst.u = src.u; dst.v = src.v;
        dst.abgr = 0xffffffff;
        for (int j = 0; j < 4; ++j) {
            dst.boneIndices[j] = src.boneIndices[j];
            dst.boneWeights[j] = src.boneWeights[j];
        }
    }

    MeshData meshData;
    meshData.vbh = bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(SkinnedVertex))),
        SkinnedVertex::ms_layout
    );
    
    meshData.numLods = static_cast<int>(importedMesh->lodIndices.size());
    if (meshData.numLods > 3) meshData.numLods = 3;
    
    for (int i = 0; i < meshData.numLods; ++i) {
        meshData.ibhLods[i] = bgfx::createIndexBuffer(
            bgfx::copy(importedMesh->lodIndices[i].data(), static_cast<uint32_t>(importedMesh->lodIndices[i].size() * sizeof(uint32_t)))
        );
        meshData.numIndicesLods[i] = static_cast<uint32_t>(importedMesh->lodIndices[i].size());
    }

    if (!importedMesh->lodIndices.empty() && !importedMesh->lodIndices[0].empty()) {
        const auto& tris = importedMesh->lodIndices[0];
        std::vector<uint32_t> lineIndices;
        lineIndices.reserve(tris.size() * 2);
        for (size_t i = 0; i + 2 < tris.size(); i += 3) {
            uint32_t i0 = tris[i];
            uint32_t i1 = tris[i+1];
            uint32_t i2 = tris[i+2];
            lineIndices.push_back(i0); lineIndices.push_back(i1);
            lineIndices.push_back(i1); lineIndices.push_back(i2);
            lineIndices.push_back(i2); lineIndices.push_back(i0);
        }
        meshData.ibhLines = bgfx::createIndexBuffer(
            bgfx::copy(lineIndices.data(), static_cast<uint32_t>(lineIndices.size() * sizeof(uint32_t))),
            BGFX_BUFFER_INDEX32
        );
        meshData.numLineIndices = static_cast<uint32_t>(lineIndices.size());
    }

    MeshHandle handle = m_nextMeshHandle++;
    m_meshes[handle] = meshData;
    m_meshGuidToHandle[guid] = handle;

    return handle;
}

static void buildDeformedCubeGeometry(
    const std::vector<Engine::Math::Vector3>& V,
    std::vector<PosColorTexCoordVertex>& outVertices,
    std::vector<uint16_t>& outIndices,
    std::vector<uint16_t>& outLineIndices
) {
    if (V.size() < 8) return;

    outVertices.clear();
    outVertices.reserve(24);
    outIndices.clear();
    outIndices.reserve(36);
    outLineIndices.clear();
    outLineIndices.reserve(72);

    auto computeNormal = [](const Engine::Math::Vector3& a, const Engine::Math::Vector3& b, const Engine::Math::Vector3& c) {
        Engine::Math::Vector3 u = b - a;
        Engine::Math::Vector3 v = c - a;
        Engine::Math::Vector3 n = u.cross(v);
        n.normalize();
        return n;
    };

    struct FaceDef {
        int idx[4]; // 0=BL, 1=BR, 2=TR, 3=TL
    };

    // 6 faces: Front, Back, Top, Bottom, Right, Left
    static const FaceDef faces[6] = {
        { { 4, 5, 6, 7 } }, // Front (Z = +)
        { { 1, 0, 3, 2 } }, // Back  (Z = -)
        { { 7, 6, 2, 3 } }, // Top   (Y = +)
        { { 0, 1, 5, 4 } }, // Bottom(Y = -)
        { { 5, 1, 2, 6 } }, // Right (X = +)
        { { 0, 4, 7, 3 } }  // Left  (X = -)
    };

    static const float uvs[4][2] = {
        { 0.0f, 0.0f },
        { 1.0f, 0.0f },
        { 1.0f, 1.0f },
        { 0.0f, 1.0f }
    };

    for (int f = 0; f < 6; ++f) {
        uint16_t baseIdx = static_cast<uint16_t>(outVertices.size());
        const auto& fd = faces[f];

        Engine::Math::Vector3 p0 = V[fd.idx[0]];
        Engine::Math::Vector3 p1 = V[fd.idx[1]];
        Engine::Math::Vector3 p2 = V[fd.idx[2]];
        Engine::Math::Vector3 p3 = V[fd.idx[3]];

        Engine::Math::Vector3 n = computeNormal(p0, p1, p3);

        const Engine::Math::Vector3 pts[4] = { p0, p1, p2, p3 };
        for (int v = 0; v < 4; ++v) {
            PosColorTexCoordVertex vert;
            vert.x = pts[v].x; vert.y = pts[v].y; vert.z = pts[v].z;
            vert.abgr = 0xffffffff;
            vert.nx = n.x; vert.ny = n.y; vert.nz = n.z;
            vert.u = uvs[v][0]; vert.v = uvs[v][1];
            outVertices.push_back(vert);
        }

        // Tri 1: (0, 1, 2), Tri 2: (0, 2, 3)
        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 1);
        outIndices.push_back(baseIdx + 2);

        outIndices.push_back(baseIdx + 0);
        outIndices.push_back(baseIdx + 2);
        outIndices.push_back(baseIdx + 3);

        // Lines for Tri 1
        outLineIndices.push_back(baseIdx + 0); outLineIndices.push_back(baseIdx + 1);
        outLineIndices.push_back(baseIdx + 1); outLineIndices.push_back(baseIdx + 2);
        outLineIndices.push_back(baseIdx + 2); outLineIndices.push_back(baseIdx + 0);

        // Lines for Tri 2
        outLineIndices.push_back(baseIdx + 0); outLineIndices.push_back(baseIdx + 2);
        outLineIndices.push_back(baseIdx + 2); outLineIndices.push_back(baseIdx + 3);
        outLineIndices.push_back(baseIdx + 3); outLineIndices.push_back(baseIdx + 0);
    }
}

MeshHandle RendererSystem::createDeformedCubeMesh(const std::vector<Engine::Math::Vector3>& localCorners) {
    if (localCorners.size() < 8) return InvalidHandle;

    std::vector<PosColorTexCoordVertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<uint16_t> lineIndices;
    buildDeformedCubeGeometry(localCorners, vertices, indices, lineIndices);

    MeshData meshData;
    meshData.vbh = bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(PosColorTexCoordVertex))),
        PosColorTexCoordVertex::ms_layout
    );
    meshData.ibhLods[0] = bgfx::createIndexBuffer(
        bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint16_t)))
    );
    meshData.numIndicesLods[0] = static_cast<uint32_t>(indices.size());
    meshData.numLods = 1;

    meshData.ibhLines = bgfx::createIndexBuffer(
        bgfx::copy(lineIndices.data(), static_cast<uint32_t>(lineIndices.size() * sizeof(uint16_t)))
    );
    meshData.numLineIndices = static_cast<uint32_t>(lineIndices.size());

    MeshHandle handle = m_nextMeshHandle++;
    m_meshes[handle] = meshData;
    return handle;
}

void RendererSystem::updateDeformedCubeMesh(MeshHandle handle, const std::vector<Engine::Math::Vector3>& localCorners) {
    auto it = m_meshes.find(handle);
    if (it == m_meshes.end()) return;

    if (localCorners.size() < 8) return;

    std::vector<PosColorTexCoordVertex> vertices;
    std::vector<uint16_t> indices;
    std::vector<uint16_t> lineIndices;
    buildDeformedCubeGeometry(localCorners, vertices, indices, lineIndices);

    if (bgfx::isValid(it->second.vbh)) {
        bgfx::destroy(it->second.vbh);
    }
    it->second.vbh = bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(PosColorTexCoordVertex))),
        PosColorTexCoordVertex::ms_layout
    );
}

MeshHandle RendererSystem::createDynamicMesh(const void* vertData, uint32_t vertSize, uint32_t vertCount, const uint32_t* indices, uint32_t numIndices, const uint32_t* lineIndices, uint32_t numLineIndices) {
    if (!vertData || vertCount == 0) return InvalidHandle;

    MeshData meshData;
    meshData.vbh = bgfx::createVertexBuffer(
        bgfx::copy(vertData, vertCount * vertSize),
        PosColorTexCoordVertex::ms_layout
    );

    if (indices && numIndices > 0) {
        std::vector<uint16_t> idx16(numIndices);
        for (uint32_t i = 0; i < numIndices; ++i) idx16[i] = static_cast<uint16_t>(indices[i]);
        meshData.ibhLods[0] = bgfx::createIndexBuffer(
            bgfx::copy(idx16.data(), static_cast<uint32_t>(idx16.size() * sizeof(uint16_t)))
        );
        meshData.numIndicesLods[0] = numIndices;
        meshData.numLods = 1;
    }

    if (lineIndices && numLineIndices > 0) {
        std::vector<uint16_t> lidx16(numLineIndices);
        for (uint32_t i = 0; i < numLineIndices; ++i) lidx16[i] = static_cast<uint16_t>(lineIndices[i]);
        meshData.ibhLines = bgfx::createIndexBuffer(
            bgfx::copy(lidx16.data(), static_cast<uint32_t>(lidx16.size() * sizeof(uint16_t)))
        );
        meshData.numLineIndices = numLineIndices;
    }

    MeshHandle handle = m_nextMeshHandle++;
    m_meshes[handle] = meshData;
    return handle;
}

void RendererSystem::updateDynamicMesh(MeshHandle handle, const void* vertData, uint32_t vertSize, uint32_t vertCount, const uint32_t* indices, uint32_t numIndices, const uint32_t* lineIndices, uint32_t numLineIndices) {
    auto it = m_meshes.find(handle);
    if (it == m_meshes.end()) return;

    if (bgfx::isValid(it->second.vbh)) {
        bgfx::destroy(it->second.vbh);
        it->second.vbh = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(it->second.ibhLods[0])) {
        bgfx::destroy(it->second.ibhLods[0]);
        it->second.ibhLods[0] = BGFX_INVALID_HANDLE;
    }
    if (bgfx::isValid(it->second.ibhLines)) {
        bgfx::destroy(it->second.ibhLines);
        it->second.ibhLines = BGFX_INVALID_HANDLE;
    }

    if (vertData && vertCount > 0) {
        it->second.vbh = bgfx::createVertexBuffer(
            bgfx::copy(vertData, vertCount * vertSize),
            PosColorTexCoordVertex::ms_layout
        );
    }

    if (indices && numIndices > 0) {
        std::vector<uint16_t> idx16(numIndices);
        for (uint32_t i = 0; i < numIndices; ++i) idx16[i] = static_cast<uint16_t>(indices[i]);
        it->second.ibhLods[0] = bgfx::createIndexBuffer(
            bgfx::copy(idx16.data(), static_cast<uint32_t>(idx16.size() * sizeof(uint16_t)))
        );
        it->second.numIndicesLods[0] = numIndices;
        it->second.numLods = 1;
    }

    if (lineIndices && numLineIndices > 0) {
        std::vector<uint16_t> lidx16(numLineIndices);
        for (uint32_t i = 0; i < numLineIndices; ++i) lidx16[i] = static_cast<uint16_t>(lineIndices[i]);
        it->second.ibhLines = bgfx::createIndexBuffer(
            bgfx::copy(lidx16.data(), static_cast<uint32_t>(lidx16.size() * sizeof(uint16_t)))
        );
        it->second.numLineIndices = numLineIndices;
    }
}

void RendererSystem::destroyMesh(MeshHandle handle) {
    auto it = m_meshes.find(handle);
    if (it != m_meshes.end()) {
        if (bgfx::isValid(it->second.vbh)) bgfx::destroy(it->second.vbh);
        for (int i = 0; i < it->second.numLods; ++i) {
            if (bgfx::isValid(it->second.ibhLods[i])) bgfx::destroy(it->second.ibhLods[i]);
        }
        if (bgfx::isValid(it->second.ibhLines)) bgfx::destroy(it->second.ibhLines);
        m_meshes.erase(it);
    }
}

} // namespace Engine::Renderer
