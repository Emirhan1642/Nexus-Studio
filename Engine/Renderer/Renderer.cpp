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

    u_albedoRoughness = bgfx::createUniform("u_albedoRoughness", bgfx::UniformType::Vec4);
    u_metallicEmissive = bgfx::createUniform("u_metallicEmissive", bgfx::UniformType::Vec4);
    u_textureFlags = bgfx::createUniform("u_textureFlags", bgfx::UniformType::Vec4);
    
    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texNormal = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);
    s_texMetallic = bgfx::createUniform("s_texMetallic", bgfx::UniformType::Sampler);
    s_texRoughness = bgfx::createUniform("s_texRoughness", bgfx::UniformType::Sampler);
    s_texBloom = bgfx::createUniform("s_texBloom", bgfx::UniformType::Sampler);
    
    u_bloomParams = bgfx::createUniform("u_bloomParams", bgfx::UniformType::Vec4);
    u_blurParams = bgfx::createUniform("u_blurParams", bgfx::UniformType::Vec4);
    u_tonemapParams = bgfx::createUniform("u_tonemapParams", bgfx::UniformType::Vec4);
    
    // Create Bone Transforms uniform (Array of 64 Mat4)
    u_boneTransforms = bgfx::createUniform("u_boneTransforms", bgfx::UniformType::Mat4, 64);
    
    // Shadow Map Setup
    u_lightMtx = bgfx::createUniform("u_lightMtx", bgfx::UniformType::Mat4);
    s_texShadow = bgfx::createUniform("s_texShadow", bgfx::UniformType::Sampler);
    
    // Create shadow map texture (D16 is widely supported for shadow maps)
    bgfx::TextureHandle shadowMapTexture = bgfx::createTexture2D(
        2048, 2048, false, 1, bgfx::TextureFormat::D16,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP
    );
    m_shadowMapFB = bgfx::createFrameBuffer(1, &shadowMapTexture, true);

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
    
    if (bgfx::isValid(vsFullscreen)) {
        if (bgfx::isValid(fsBloomThreshold)) m_bloomThresholdProgram = bgfx::createProgram(vsFullscreen, fsBloomThreshold, true);
        else bgfx::destroy(vsFullscreen); // Will recreate below if needed, wait bgfx::createProgram destroys them if true is passed.
    }
    
    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsBloomBlur)) {
        m_bloomBlurProgram = bgfx::createProgram(vsFullscreen, fsBloomBlur, true);
    }

    vsFullscreen = loadShader("Engine/Renderer/Shaders/vs_fullscreen.bin");
    if (bgfx::isValid(vsFullscreen) && bgfx::isValid(fsTonemap)) {
        m_tonemapProgram = bgfx::createProgram(vsFullscreen, fsTonemap, true);
    }

    bgfx::setViewClear(View_MainColor, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewClear(View_ShadowPass, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
}

void RendererSystem::shutdown() {
    bgfx::destroy(m_ibh);
    bgfx::destroy(m_vbh);
    bgfx::destroy(m_program);
    if (bgfx::isValid(m_shadowProgram)) bgfx::destroy(m_shadowProgram);
    if (bgfx::isValid(m_skinnedShadowProgram)) bgfx::destroy(m_skinnedShadowProgram);
    
    if (bgfx::isValid(m_shadowMapFB)) bgfx::destroy(m_shadowMapFB);
    if (bgfx::isValid(u_lightMtx)) bgfx::destroy(u_lightMtx);
    if (bgfx::isValid(s_texShadow)) bgfx::destroy(s_texShadow);
    
    bgfx::destroy(u_albedoRoughness);
    bgfx::destroy(u_metallicEmissive);
    bgfx::destroy(u_textureFlags);
    
    bgfx::destroy(s_texColor);
    bgfx::destroy(s_texNormal);
    bgfx::destroy(s_texMetallic);
    bgfx::destroy(s_texRoughness);
    bgfx::destroy(s_texBloom);
    
    bgfx::destroy(u_bloomParams);
    bgfx::destroy(u_blurParams);
    bgfx::destroy(u_tonemapParams);
    
    if (bgfx::isValid(m_bloomThresholdProgram)) bgfx::destroy(m_bloomThresholdProgram);
    if (bgfx::isValid(m_bloomBlurProgram)) bgfx::destroy(m_bloomBlurProgram);
    if (bgfx::isValid(m_tonemapProgram)) bgfx::destroy(m_tonemapProgram);
    
    if (bgfx::isValid(m_hdrFB)) bgfx::destroy(m_hdrFB);
    for (int i = 0; i < 5; ++i) {
        if (bgfx::isValid(m_bloomFBs[i])) bgfx::destroy(m_bloomFBs[i]);
        if (bgfx::isValid(m_bloomBlurFBs[i])) bgfx::destroy(m_bloomBlurFBs[i]);
    }
    
    if (bgfx::isValid(u_boneTransforms)) {
        bgfx::destroy(u_boneTransforms);
    }

    for (auto& [path, handle] : m_textureCache) {
        if (bgfx::isValid(handle)) bgfx::destroy(handle);
    }
    m_textureCache.clear();
    
    for (auto& [handle, meshData] : m_meshes) {
        if (bgfx::isValid(meshData.vbh)) bgfx::destroy(meshData.vbh);
        if (bgfx::isValid(meshData.ibh)) bgfx::destroy(meshData.ibh);
    }
    m_meshes.clear();
    m_meshGuidToHandle.clear();

    if (bgfx::isValid(m_defaultAlbedo)) bgfx::destroy(m_defaultAlbedo);
    if (bgfx::isValid(m_defaultNormal)) bgfx::destroy(m_defaultNormal);
    if (bgfx::isValid(m_defaultMetallic)) bgfx::destroy(m_defaultMetallic);
    if (bgfx::isValid(m_defaultRoughness)) bgfx::destroy(m_defaultRoughness);
}

void RendererSystem::renderFrame(const Camera& camera, int width, int height, bgfx::FrameBufferHandle fb) {
    // -----------------------------------------
    // PASS 1: SHADOW MAP PASS
    // -----------------------------------------
    const int shadowMapSize = 2048;
    bgfx::setViewRect(View_ShadowPass, 0, 0, shadowMapSize, shadowMapSize);
    bgfx::setViewFrameBuffer(View_ShadowPass, m_shadowMapFB);
    bgfx::setViewClear(View_ShadowPass, BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    // Directional Light Settings (Hardcoded for MVP)
    Engine::Math::Vector3 lightDir = {1.0f, 1.0f, -1.0f}; // Must match L in fs_pbr.sc
    Engine::Math::Vector3 lightPos = { -lightDir.x * 50.0f, -lightDir.y * 50.0f, -lightDir.z * 50.0f };
    
    float lightView[16];
    bx::mtxLookAt(lightView, 
        {lightPos.x, lightPos.y, lightPos.z},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}
    );

    float lightProj[16];
    bx::mtxOrtho(lightProj, -30.0f, 30.0f, -30.0f, 30.0f, 1.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
    bgfx::setViewTransform(View_ShadowPass, lightView, lightProj);

    // Calculate lightMtx for shadow sampling in Main Pass
    float lightVP[16];
    bx::mtxMul(lightVP, lightView, lightProj);

    // Some APIs need half-pixel offset or NDC adjustment for texture sampling.
    // bx::mtxMul usually handles standard projection. For shadow mapping, 
    // it's common to pass the raw VP matrix and do the *0.5+0.5 bias in the shader.
    bgfx::setUniform(u_lightMtx, lightVP);

    bgfx::touch(View_ShadowPass);

    // Fetch Proxies
    RenderScene::instance().flushDirtyTransforms();
    const auto& proxies = RenderScene::instance().getProxies();

    for (const auto& proxy : proxies) {
        if (!proxy.visible) continue;
        
        bgfx::ProgramHandle prog = proxy.boneTransforms.empty() ? m_shadowProgram : m_skinnedShadowProgram;
        if (!bgfx::isValid(prog)) continue;

        bgfx::setTransform(proxy.worldTransform.m.data());

        if (proxy.mesh != InvalidHandle) {
            auto it = m_meshes.find(proxy.mesh);
            if (it != m_meshes.end()) {
                bgfx::setVertexBuffer(0, it->second.vbh);
                bgfx::setIndexBuffer(it->second.ibh);
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

        bgfx::setState(BGFX_STATE_DEFAULT | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_WRITE_Z);
        bgfx::submit(View_ShadowPass, prog);
    }

    // -----------------------------------------
    // FRAMEBUFFER RESIZE HANDLING
    // -----------------------------------------
    if (m_width != width || m_height != height) {
        m_width = width;
        m_height = height;

        if (bgfx::isValid(m_hdrFB)) bgfx::destroy(m_hdrFB);
        for (int i = 0; i < 5; ++i) {
            if (bgfx::isValid(m_bloomFBs[i])) bgfx::destroy(m_bloomFBs[i]);
            if (bgfx::isValid(m_bloomBlurFBs[i])) bgfx::destroy(m_bloomBlurFBs[i]);
        }

        bgfx::TextureHandle hdrColorTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle hdrDepthTex = bgfx::createTexture2D(width, height, false, 1, bgfx::TextureFormat::D24, BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
        bgfx::TextureHandle hdrTextures[] = { hdrColorTex, hdrDepthTex };
        m_hdrFB = bgfx::createFrameBuffer(2, hdrTextures, true);

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

    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 proj = camera.getProjectionMatrix(float(width) / float(height));
    bgfx::setViewTransform(View_MainColor, view.m.data(), proj.m.data());

    bgfx::touch(View_MainColor);

    bgfx::TextureHandle shadowTex = bgfx::getTexture(m_shadowMapFB, 0);

    for (const auto& proxy : proxies) {
        if (!proxy.visible) continue;
        if (!bgfx::isValid(m_program)) continue;

        bgfx::setTransform(proxy.worldTransform.m.data());

        if (proxy.mesh != InvalidHandle) {
            auto it = m_meshes.find(proxy.mesh);
            if (it != m_meshes.end()) {
                bgfx::setVertexBuffer(0, it->second.vbh);
                bgfx::setIndexBuffer(it->second.ibh);
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

        float albedoRoughness[4] = { proxy.material.albedo.x, proxy.material.albedo.y, proxy.material.albedo.z, proxy.material.roughness };
        float metallicEmissive[4] = { proxy.material.metallic, proxy.material.emissiveStrength, 0.0f, 0.0f };
        float textureFlags[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

        if (bgfx::isValid(proxy.material.albedoTexture)) {
            bgfx::setTexture(0, s_texColor, proxy.material.albedoTexture);
            textureFlags[0] = 1.0f;
        } else {
            bgfx::setTexture(0, s_texColor, m_defaultAlbedo);
        }

        if (bgfx::isValid(proxy.material.normalTexture)) {
            bgfx::setTexture(1, s_texNormal, proxy.material.normalTexture);
            textureFlags[1] = 1.0f;
        } else {
            bgfx::setTexture(1, s_texNormal, m_defaultNormal);
        }

        if (bgfx::isValid(proxy.material.metallicTexture)) {
            bgfx::setTexture(2, s_texMetallic, proxy.material.metallicTexture);
            textureFlags[2] = 1.0f;
        } else {
            bgfx::setTexture(2, s_texMetallic, m_defaultMetallic);
        }

        if (bgfx::isValid(proxy.material.roughnessTexture)) {
            bgfx::setTexture(3, s_texRoughness, proxy.material.roughnessTexture);
            textureFlags[3] = 1.0f;
        } else {
            bgfx::setTexture(3, s_texRoughness, m_defaultRoughness);
        }

        // Bind Shadow Map
        bgfx::setTexture(4, s_texShadow, shadowTex);

        bgfx::setUniform(u_albedoRoughness, albedoRoughness);
        bgfx::setUniform(u_metallicEmissive, metallicEmissive);
        bgfx::setUniform(u_textureFlags, textureFlags);

        bgfx::setState(BGFX_STATE_DEFAULT);
        bgfx::submit(View_MainColor, m_program);
    }
    
    // -----------------------------------------
    // PASS 3: POST-PROCESSING (Bloom & Tonemap)
    // -----------------------------------------
    bgfx::TextureHandle hdrColorTex = bgfx::getTexture(m_hdrFB, 0);
    
    // 3.1 Bloom Threshold
    float bloomParams[4] = { 1.5f, 0.0f, 0.0f, 0.0f }; // threshold
    bgfx::setUniform(u_bloomParams, bloomParams);
    bgfx::setTexture(0, s_texColor, hdrColorTex);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setViewRect(View_BloomThreshold, 0, 0, uint16_t(bx::max(1, width / 2)), uint16_t(bx::max(1, height / 2)));
    bgfx::setViewFrameBuffer(View_BloomThreshold, m_bloomFBs[0]);
    bgfx::setViewClear(View_BloomThreshold, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
    // Draw full screen quad manually using our vertex buffer (or just use 3 vertices in bgfx, but here we can just bind cube's front face)
    bgfx::setVertexBuffer(0, m_vbh, 0, 4);
    bgfx::setIndexBuffer(m_ibh, 0, 6);
    bgfx::submit(View_BloomThreshold, m_bloomThresholdProgram);

    // 3.2 Bloom Downsample & Blur (Ping-Pong)
    int bw = width / 2;
    int bh = height / 2;
    bgfx::ViewId currentViewId = 3;
    
    for (int i = 0; i < 5; ++i) {
        // Blur X (Write to bloomBlurFBs[i], read from bloomFBs[i])
        float blurXParams[4] = { 1.0f / (float)bw, 0.0f, 0.0f, 0.0f };
        bgfx::setUniform(u_blurParams, blurXParams);
        bgfx::setTexture(0, s_texColor, bgfx::getTexture(m_bloomFBs[i], 0));
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setViewRect(currentViewId, 0, 0, uint16_t(bw), uint16_t(bh));
        bgfx::setViewFrameBuffer(currentViewId, m_bloomBlurFBs[i]);
        bgfx::setViewClear(currentViewId, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
        bgfx::setVertexBuffer(0, m_vbh, 0, 4);
        bgfx::setIndexBuffer(m_ibh, 0, 6);
        bgfx::submit(currentViewId++, m_bloomBlurProgram);

        // Blur Y (Write back to bloomFBs[i], read from bloomBlurFBs[i])
        float blurYParams[4] = { 0.0f, 1.0f / (float)bh, 0.0f, 0.0f };
        bgfx::setUniform(u_blurParams, blurYParams);
        bgfx::setTexture(0, s_texColor, bgfx::getTexture(m_bloomBlurFBs[i], 0));
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
        bgfx::setViewRect(currentViewId, 0, 0, uint16_t(bw), uint16_t(bh));
        bgfx::setViewFrameBuffer(currentViewId, m_bloomFBs[i]);
        bgfx::setViewClear(currentViewId, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
        bgfx::setVertexBuffer(0, m_vbh, 0, 4);
        bgfx::setIndexBuffer(m_ibh, 0, 6);
        bgfx::submit(currentViewId++, m_bloomBlurProgram);
        
        // Prepare sizes for next mip level (if we were chaining them properly, we'd sample from i into i+1 next). 
        // Here we just blur the 0th level for MVP bloom. Wait! We need to downscale.
        // Actually, to make a true bloom, we downsample M levels, then upsample. 
        // For MVP, we'll just blur the level 0 heavily.
        if (i < 4) {
            bw = bx::max(1, bw / 2);
            bh = bx::max(1, bh / 2);
            
            // Downsample: read from bloomFBs[i], write to bloomFBs[i+1]
            float downsampleBlur[4] = { 1.0f / (float)bw, 0.0f, 0.0f, 0.0f }; // simple copy basically
            bgfx::setUniform(u_blurParams, downsampleBlur);
            bgfx::setTexture(0, s_texColor, bgfx::getTexture(m_bloomFBs[i], 0));
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setViewRect(currentViewId, 0, 0, uint16_t(bw), uint16_t(bh));
            bgfx::setViewFrameBuffer(currentViewId, m_bloomFBs[i+1]);
            bgfx::setViewClear(currentViewId, BGFX_CLEAR_COLOR, 0x00000000, 1.0f, 0);
            bgfx::setVertexBuffer(0, m_vbh, 0, 4);
            bgfx::setIndexBuffer(m_ibh, 0, 6);
            bgfx::submit(currentViewId++, m_bloomBlurProgram);
        }
    }

    // 3.3 Tonemap & Additive Blend (Render to Backbuffer)
    float tonemapParams[4] = { 1.0f /*exposure*/, 1.0f /*bloom int*/, 0.0f, 0.0f };
    bgfx::setUniform(u_tonemapParams, tonemapParams);
    bgfx::setTexture(0, s_texColor, hdrColorTex);
    bgfx::setTexture(1, s_texBloom, bgfx::getTexture(m_bloomFBs[0], 0)); // Add the highest res blurred bloom

    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    bgfx::setViewRect(View_Tonemap, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewFrameBuffer(View_Tonemap, fb); // Default backbuffer
    bgfx::setViewClear(View_Tonemap, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x000000ff, 1.0f, 0);
    
    bgfx::setVertexBuffer(0, m_vbh, 0, 4);
    bgfx::setIndexBuffer(m_ibh, 0, 6);
    bgfx::submit(View_Tonemap, m_tonemapProgram);
    
    bgfx::frame();
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

    if (importedMesh->vertices.empty() || importedMesh->indices.empty()) {
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
    meshData.ibh = bgfx::createIndexBuffer(
        bgfx::copy(importedMesh->indices.data(), static_cast<uint32_t>(importedMesh->indices.size() * sizeof(uint32_t)))
    );
    meshData.numIndices = static_cast<uint32_t>(importedMesh->indices.size());

    MeshHandle handle = m_nextMeshHandle++;
    m_meshes[handle] = meshData;
    m_meshGuidToHandle[guid] = handle;

    return handle;
}

} // namespace Engine::Renderer
