#include "Renderer.h"
#include <bx/timer.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <bx/allocator.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>

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
    
    m_vbh = bgfx::createVertexBuffer(bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)), PosColorTexCoordVertex::ms_layout);
    m_ibh = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices)));
    
    u_albedoRoughness = bgfx::createUniform("u_albedoRoughness", bgfx::UniformType::Vec4);
    u_metallicEmissive = bgfx::createUniform("u_metallicEmissive", bgfx::UniformType::Vec4);
    u_textureFlags = bgfx::createUniform("u_textureFlags", bgfx::UniformType::Vec4);

    s_texColor = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
    s_texNormal = bgfx::createUniform("s_texNormal", bgfx::UniformType::Sampler);
    s_texMetallic = bgfx::createUniform("s_texMetallic", bgfx::UniformType::Sampler);
    s_texRoughness = bgfx::createUniform("s_texRoughness", bgfx::UniformType::Sampler);

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
        m_program = bgfx::createProgram(vsh, fsh, true); // true = destroy shaders after program creation
    }

    bgfx::setViewClear(View_MainColor, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
}

void RendererSystem::shutdown() {
    if (bgfx::isValid(m_program)) {
        bgfx::destroy(m_program);
    }
    bgfx::destroy(m_ibh);
    bgfx::destroy(m_vbh);
    bgfx::destroy(u_albedoRoughness);
    bgfx::destroy(u_metallicEmissive);
    bgfx::destroy(u_textureFlags);
    bgfx::destroy(s_texColor);
    bgfx::destroy(s_texNormal);
    bgfx::destroy(s_texMetallic);
    bgfx::destroy(s_texRoughness);

    bgfx::destroy(m_defaultAlbedo);
    bgfx::destroy(m_defaultNormal);
    bgfx::destroy(m_defaultMetallic);
    bgfx::destroy(m_defaultRoughness);

    for (auto& pair : m_textureCache) {
        if (bgfx::isValid(pair.second)) {
            bgfx::destroy(pair.second);
        }
    }
    m_textureCache.clear();
}

void RendererSystem::renderFrame(const Camera& camera, int width, int height, bgfx::FrameBufferHandle fb) {
    // 1. Ekran boyutlarını bgfx'e bildir.
    bgfx::setViewRect(View_MainColor, 0, 0, uint16_t(width), uint16_t(height));
    bgfx::setViewFrameBuffer(View_MainColor, fb);

    // clear background for the view
    bgfx::setViewClear(View_MainColor, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

    // 2. Kamera matrislerini ayarla.
    Engine::Math::Matrix4 view = camera.getViewMatrix();
    Engine::Math::Matrix4 proj = camera.getProjectionMatrix(float(width) / float(height));
    bgfx::setViewTransform(View_MainColor, view.m.data(), proj.m.data());

    // Dokunmak (viewport clear yapmak için boş bir çizim tetikler)
    bgfx::touch(View_MainColor);

    // 3. RenderScene'deki güncellemeleri çek.
    RenderScene::instance().flushDirtyTransforms();

    const auto& proxies = RenderScene::instance().getProxies();

    // 4. Her bir proxy'yi çiz (Basit Forward Culling/Draw)
    for (const auto& proxy : proxies) {
        if (!proxy.visible) continue;
        if (!bgfx::isValid(m_program)) continue; // Shader yüklenmediyse çizim yapma

        // Transform'u gönder
        bgfx::setTransform(proxy.worldTransform.m.data());

        // Vertex/Index buffer
        bgfx::setVertexBuffer(0, m_vbh);
        bgfx::setIndexBuffer(m_ibh);

        // Uniforms and Textures
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

        bgfx::setUniform(u_albedoRoughness, albedoRoughness);
        bgfx::setUniform(u_metallicEmissive, metallicEmissive);
        bgfx::setUniform(u_textureFlags, textureFlags);

        // Durumlar: Derinlik testi vs.
        bgfx::setState(BGFX_STATE_DEFAULT);

        bgfx::submit(View_MainColor, m_program);
    }
}

} // namespace Engine::Renderer
