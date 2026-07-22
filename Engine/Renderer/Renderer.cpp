#include "Renderer.h"
#include <bx/timer.h>
#include <iostream>
#include <fstream>
#include <string>

namespace Engine::Renderer {

struct PosColorVertex {
    float x, y, z;
    uint32_t abgr;
    float nx, ny, nz; // Normal

    static void init() {
        ms_layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .end();
    }
    static bgfx::VertexLayout ms_layout;
};

bgfx::VertexLayout PosColorVertex::ms_layout;

static const PosColorVertex s_cubeVertices[] = {
    // Front face (Z = 1)
    { -1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f }, // 0: Top-Left
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f }, // 1: Top-Right
    { -1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f }, // 2: Bottom-Left
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f,  0.0f,  1.0f }, // 3: Bottom-Right
    // Back face (Z = -1)
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f }, // 4: Top-Left
    { -1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f }, // 5: Top-Right
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f }, // 6: Bottom-Left
    { -1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f,  0.0f, -1.0f }, // 7: Bottom-Right
    // Top face (Y = 1)
    { -1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f }, // 8: Top-Left
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f }, // 9: Top-Right
    { -1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f }, // 10: Bottom-Left
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  0.0f,  1.0f,  0.0f }, // 11: Bottom-Right
    // Bottom face (Y = -1)
    { -1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f }, // 12: Top-Left
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f }, // 13: Top-Right
    { -1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f }, // 14: Bottom-Left
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  0.0f, -1.0f,  0.0f }, // 15: Bottom-Right
    // Right face (X = 1)
    {  1.0f,  1.0f,  1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f }, // 16: Top-Left
    {  1.0f,  1.0f, -1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f }, // 17: Top-Right
    {  1.0f, -1.0f,  1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f }, // 18: Bottom-Left
    {  1.0f, -1.0f, -1.0f, 0xffffffff,  1.0f,  0.0f,  0.0f }, // 19: Bottom-Right
    // Left face (X = -1)
    { -1.0f,  1.0f, -1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f }, // 20: Top-Left
    { -1.0f,  1.0f,  1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f }, // 21: Top-Right
    { -1.0f, -1.0f, -1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f }, // 22: Bottom-Left
    { -1.0f, -1.0f,  1.0f, 0xffffffff, -1.0f,  0.0f,  0.0f }, // 23: Bottom-Right
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
    PosColorVertex::init();
    
    m_vbh = bgfx::createVertexBuffer(bgfx::makeRef(s_cubeVertices, sizeof(s_cubeVertices)), PosColorVertex::ms_layout);
    m_ibh = bgfx::createIndexBuffer(bgfx::makeRef(s_cubeIndices, sizeof(s_cubeIndices)));
    
    u_albedoRoughness = bgfx::createUniform("u_albedoRoughness", bgfx::UniformType::Vec4);
    u_metallicEmissive = bgfx::createUniform("u_metallicEmissive", bgfx::UniformType::Vec4);

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

        // Uniforms (Şimdilik statik bir materyal)
        float albedoRoughness[4] = {0.8f, 0.8f, 0.8f, 0.5f}; // Gri renk, orta pürüzlülük
        float metallicEmissive[4] = {0.0f, 0.0f, 0.0f, 0.0f}; // Metalik değil, emissive değil
        bgfx::setUniform(u_albedoRoughness, albedoRoughness);
        bgfx::setUniform(u_metallicEmissive, metallicEmissive);

        // Durumlar: Derinlik testi vs.
        bgfx::setState(BGFX_STATE_DEFAULT);

        bgfx::submit(View_MainColor, m_program);
    }
}

} // namespace Engine::Renderer
