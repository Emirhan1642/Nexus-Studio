#include "ThumbnailRenderer.h"
#include "ThumbnailCache.h"
#include "../Renderer/Renderer.h"
#include <iostream>

namespace Engine::Assets {

// --- ThumbnailRenderer ---
ThumbnailRenderer::ThumbnailRenderer() {
    m_colorTex = bgfx::createTexture2D(128, 128, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
    m_depthTex = bgfx::createTexture2D(128, 128, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);

    bgfx::TextureHandle attachments[] = { m_colorTex, m_depthTex };
    m_frameBuffer = bgfx::createFrameBuffer(2, attachments, true);
}

ThumbnailRenderer::~ThumbnailRenderer() {
    if (bgfx::isValid(m_frameBuffer)) {
        bgfx::destroy(m_frameBuffer);
    }
}

bgfx::TextureHandle ThumbnailRenderer::renderThumbnail(AssetGuid guid) {
    const AssetMetadata* meta = AssetDatabase::instance().find(guid);
    if (meta && meta->importerType == "Texture") {
        return Engine::Renderer::RendererSystem::instance().getTexture(
            AssetDatabase::instance().getAbsolutePath(meta->relativePath)
        );
    }
    
    // For MVP, if it's a mesh, we just return a default texture
    // In a real scenario we'd call RendererSystem::renderSingleObject with the VBH/IBH from the AssetDatabase
    // But since Assimp isn't fully integrated for Mesh rendering here, we fallback to default
    return Engine::Renderer::RendererSystem::instance().getTexture("");
}

// --- ThumbnailCache ---
void ThumbnailCache::initialize() {
    m_renderer = new ThumbnailRenderer();
    m_placeholder = Engine::Renderer::RendererSystem::instance().getTexture("");
}

void ThumbnailCache::shutdown() {
    delete m_renderer;
    m_renderer = nullptr;
}

bgfx::TextureHandle ThumbnailCache::get(AssetGuid guid) {
    auto it = m_cache.find(guid.toString());
    if (it != m_cache.end()) return it->second;

    m_pendingRenders.push_back(guid);
    return m_placeholder;
}

void ThumbnailCache::invalidate(AssetGuid guid) {
    m_cache.erase(guid.toString());
}

void ThumbnailCache::processPendingRenders(int maxPerFrame) {
    if (!m_renderer) return;

    int processed = 0;
    while (processed < maxPerFrame && !m_pendingRenders.empty()) {
        AssetGuid guid = m_pendingRenders.back();
        m_pendingRenders.pop_back();

        m_cache[guid.toString()] = m_renderer->renderThumbnail(guid);
        processed++;
    }
}

} // namespace Engine::Assets
