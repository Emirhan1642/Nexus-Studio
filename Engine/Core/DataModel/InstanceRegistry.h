#pragma once
#include "Instance.h"
#include <unordered_map>
#include <memory>
#include <mutex>

class InstanceRegistry {
public:
    static InstanceRegistry& instance() {
        static InstanceRegistry s_instance;
        return s_instance;
    }

    void registerInstance(const std::shared_ptr<Instance>& inst);
    void unregisterInstance(InstanceId id);
    std::shared_ptr<Instance> findById(InstanceId id) const;
    
    std::vector<std::shared_ptr<Instance>> getAllInstances() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::shared_ptr<Instance>> result;
        for (const auto& [id, weakPtr] : m_registry) {
            if (auto ptr = weakPtr.lock()) {
                result.push_back(ptr);
            }
        }
        return result;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_registry.clear();
    }

private:
    InstanceRegistry() = default;
    
    mutable std::mutex m_mutex;
    std::unordered_map<InstanceId, std::weak_ptr<Instance>> m_registry;
};
