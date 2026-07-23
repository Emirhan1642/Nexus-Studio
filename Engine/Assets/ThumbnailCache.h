#pragma once
#include "AssetDatabase.h"
#include <bgfx/bgfx.h>
#include <unordered_map>
#include <vector>

namespace Engine::Assets {

class ThumbnailRenderer;

class ThumbnailCache {
public:
    static ThumbnailCache& instance() { static ThumbnailCache cache; return cache; }

    void initialize();
    void shutdown();

    bgfx::TextureHandle get(AssetGuid guid);
    void invalidate(AssetGuid guid);
    void processPendingRenders(int maxPerFrame = 2);

private:
    std::unordered_map<std::string, bgfx::TextureHandle> m_cache;
    std::vector<AssetGuid> m_pendingRenders;
    bgfx::TextureHandle m_placeholder = BGFX_INVALID_HANDLE;
    ThumbnailRenderer* m_renderer = nullptr;
};

} // namespace Engine::Assets
