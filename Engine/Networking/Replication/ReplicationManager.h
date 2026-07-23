#pragma once
#include "SpatialGrid.h"
#include "RelevancyTracker.h"
#include "DormancyManager.h"
#include "PriorityCalculator.h"
#include "AlwaysRelevantRegistry.h"
#include "../Transport/NetworkServer.h"
#include <unordered_map>
#include <string>
#include <vector>

namespace Engine::Networking {

    class ReplicationManager {
    public:
        static ReplicationManager& instance() {
            static ReplicationManager s_instance;
            return s_instance;
        }

        void markPropertyDirty(InstanceId id, const std::string& propertyName);
        void flushToAllClients(float deltaTime);

    private:
        ReplicationManager() = default;
        ~ReplicationManager() = default;

        SpatialGrid m_spatialGrid;
        RelevancyTracker m_relevancyTracker;
        DormancyManager m_dormancyManager;
        PriorityCalculator m_priorityCalculator;

        std::unordered_map<InstanceId, std::vector<std::string>> m_dirtyProperties;
        
        // Mock server time
        float m_currentServerTime = 0.0f;
        float m_replicationTimer = 0.0f;
        static constexpr float REPLICATION_INTERVAL = 0.05f; // 20Hz
        static constexpr int MAX_UPDATES_PER_CLIENT_PER_TICK = 64;
        static constexpr int INITIAL_SYNC_BATCH_SIZE = 10;
        
        std::unordered_map<uint32_t, ClientRelevancyState> m_clientStates;
        std::unordered_map<uint32_t, std::unordered_map<InstanceId, float>> m_lastSentTimes;
    };
}
