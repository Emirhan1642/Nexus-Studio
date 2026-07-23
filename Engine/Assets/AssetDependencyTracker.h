#pragma once
#include "AssetDatabase.h"
#include <unordered_map>
#include <set>
#include <cstdint>

namespace Engine::Assets {

class AssetDependencyTracker {
public:
    static AssetDependencyTracker& instance() { static AssetDependencyTracker t; return t; }

    void registerUsage(AssetGuid assetGuid, uint64_t userInstanceId) {
        m_usageMap[assetGuid.toString()].insert(userInstanceId);
    }

    void unregisterUsage(AssetGuid assetGuid, uint64_t userInstanceId) {
        m_usageMap[assetGuid.toString()].erase(userInstanceId);
    }

    const std::set<uint64_t>& getUsers(AssetGuid assetGuid) const {
        static std::set<uint64_t> empty;
        auto it = m_usageMap.find(assetGuid.toString());
        return it != m_usageMap.end() ? it->second : empty;
    }

private:
    std::unordered_map<std::string, std::set<uint64_t>> m_usageMap;
};

} // namespace Engine::Assets
