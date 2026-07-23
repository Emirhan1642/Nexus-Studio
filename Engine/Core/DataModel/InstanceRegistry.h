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

private:
    InstanceRegistry() = default;
    
    mutable std::mutex m_mutex;
    std::unordered_map<InstanceId, std::weak_ptr<Instance>> m_registry;
};
