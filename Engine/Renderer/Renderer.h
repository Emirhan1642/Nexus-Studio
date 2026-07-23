#pragma once
#include "SceneGraph/RenderScene.h"
#include "Camera.h"
#include <bgfx/bgfx.h>
#include <vector>
#include <string>
#include <unordered_map>

namespace Engine::Renderer {

enum RenderView : bgfx::ViewId {
    View_MainColor = 0,
    // Diğer view'lar ileride eklenecek (ShadowMap vb.)
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

private:
    RendererSystem() = default;

    // Hardcoded küp nesnesi için VBO/IBO
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;

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

    void createDefaultResources();
    void destroyDefaultResources();
};

} // namespace Engine::Renderer
