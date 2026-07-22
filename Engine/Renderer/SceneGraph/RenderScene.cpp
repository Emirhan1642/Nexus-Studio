#include "RenderScene.h"

namespace Engine::Renderer {

uint32_t RenderScene::registerProxy(InstanceId ownerId, const RenderProxy& initial) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    uint32_t index = static_cast<uint32_t>(m_proxies.size());
    m_proxies.push_back(initial);
    m_ownerToIndex[ownerId] = index;
    return index;
}

void RenderScene::unregisterProxy(uint32_t proxyIndex) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (proxyIndex < m_proxies.size()) {
        m_proxies[proxyIndex].visible = false;
        // Aslında swap-and-pop yapılabilir, ancak indexlerin tutarlılığı açısından
        // Faz 2 MVP'sinde sadece görünmez yapıyoruz. Daha gelişmiş bir id allocation 
        // sistemi (örneğin free list) eklenebilir.
    }
}

void RenderScene::markDirty(uint32_t proxyIndex, const Engine::Math::Matrix4& newTransform) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dirtyEntries.push_back({ proxyIndex, newTransform });
}

void RenderScene::flushDirtyTransforms() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& entry : m_dirtyEntries) {
        if (entry.proxyIndex < m_proxies.size()) {
            m_proxies[entry.proxyIndex].worldTransform = entry.transform;
        }
    }
    m_dirtyEntries.clear();
}

} // namespace Engine::Renderer
