#include "InstanceRegistry.h"

void InstanceRegistry::registerInstance(const std::shared_ptr<Instance>& inst) {
    if (!inst) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry[inst->getInstanceId()] = inst;
}

void InstanceRegistry::unregisterInstance(InstanceId id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registry.erase(id);
}

std::shared_ptr<Instance> InstanceRegistry::findById(InstanceId id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_registry.find(id);
    if (it != m_registry.end()) {
        return it->second.lock();
    }
    return nullptr;
}
