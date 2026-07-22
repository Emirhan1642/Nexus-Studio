#pragma once
#include "SceneGraph/RenderScene.h"
#include "Camera.h"
#include <bgfx/bgfx.h>
#include <vector>

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

private:
    RendererSystem() = default;

    // Hardcoded küp nesnesi için VBO/IBO
    bgfx::VertexBufferHandle m_vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle m_ibh = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;

    // Uniforms
    bgfx::UniformHandle u_albedoRoughness = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_metallicEmissive = BGFX_INVALID_HANDLE;

    void createDefaultResources();
    void destroyDefaultResources();
};

} // namespace Engine::Renderer
