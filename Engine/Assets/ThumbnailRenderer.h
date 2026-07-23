#pragma once
#include <bgfx/bgfx.h>
#include "AssetDatabase.h"

namespace Engine::Assets {

class ThumbnailRenderer {
public:
    ThumbnailRenderer();
    ~ThumbnailRenderer();

    bgfx::TextureHandle renderThumbnail(AssetGuid guid);

private:
    bgfx::FrameBufferHandle m_frameBuffer = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_colorTex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle m_depthTex = BGFX_INVALID_HANDLE;
};

} // namespace Engine::Assets
