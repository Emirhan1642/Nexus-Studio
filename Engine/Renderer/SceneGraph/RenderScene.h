#pragma once
#include "RenderProxy.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include "../../Core/DataModel/Instance.h"

namespace Engine::Renderer {

class RenderScene {
public:
    static RenderScene& instance() {
        static RenderScene s_instance;
        return s_instance;
    }

    uint32_t registerProxy(InstanceId ownerId, const RenderProxy& initial);
    void unregisterProxy(uint32_t proxyIndex);
    
    // İş parçacığı güvenli olması açısından lock eklenebilir, şimdilik basit bir yapı kuruyoruz.
    void markDirty(uint32_t proxyIndex, const Engine::Math::Matrix4& newTransform);

    // Renderer'in çizebilmesi için proxy listesini döndürür.
    const std::vector<RenderProxy>& getProxies() const { return m_proxies; }
    
    // Her kare başı dirty olan proxy'leri asıl proxy listesine uygular.
    void flushDirtyTransforms();

private:
    RenderScene() = default;

    std::vector<RenderProxy> m_proxies;
    std::unordered_map<InstanceId, uint32_t> m_ownerToIndex;
    
    struct DirtyEntry {
        uint32_t proxyIndex;
        Engine::Math::Matrix4 transform;
    };
    std::vector<DirtyEntry> m_dirtyEntries;
    std::mutex m_mutex;
};

} // namespace Engine::Renderer
