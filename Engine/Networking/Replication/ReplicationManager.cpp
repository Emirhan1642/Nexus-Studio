#define NOMINMAX
#include "ReplicationManager.h"
#include <iostream>

namespace Engine::Networking {

    void ReplicationManager::markPropertyDirty(InstanceId id, const std::string& propertyName) {
        if (std::find(m_dirtyProperties[id].begin(), m_dirtyProperties[id].end(), propertyName) == m_dirtyProperties[id].end()) {
            m_dirtyProperties[id].push_back(propertyName);
        }
        m_dormancyManager.onInstanceDirty(id, m_currentServerTime);
    }

    void ReplicationManager::flushToAllClients(float deltaTime) {
        m_currentServerTime += deltaTime;
        m_replicationTimer += deltaTime;

        if (m_replicationTimer < REPLICATION_INTERVAL) return;
        m_replicationTimer -= REPLICATION_INTERVAL;

        m_dormancyManager.updateDormancy(m_currentServerTime, m_spatialGrid);

        // In a real scenario, we iterate over connected clients from NetworkServer.
        // For demonstration, assume m_clientStates is populated when a client joins.

        for (auto& [clientId, state] : m_clientStates) {
            // 1. Initial Sync Throttling
            int sentInitial = 0;
            while (!state.pendingInitialSync.empty() && sentInitial < INITIAL_SYNC_BATCH_SIZE) {
                InstanceId id = state.pendingInitialSync.back();
                state.pendingInitialSync.pop_back();
                // NetworkServer::instance().sendTo(..., buildFullPacket(id));
                sentInitial++;
            }

            // 2. Always Relevant Objects
            for (InstanceId id : AlwaysRelevantRegistry::instance().getAll()) {
                if (m_dirtyProperties.contains(id)) {
                    // Send updates
                }
            }

            // 3. Spatial Grid & Priority calculation
            // Let's assume player's center is (0,0,0) for now.
            Math::Vector3 playerPos(0.0f, 0.0f, 0.0f);
            
            std::vector<InstanceId> nearby = m_spatialGrid.queryRadius(playerPos, 3);
            
            std::vector<std::pair<InstanceId, float>> prioritized;
            for (InstanceId id : nearby) {
                if (m_dormancyManager.isDormant(id)) continue;
                
                // distance check
                float distance = 0.0f; // Mock distance
                RelevancyTracker::Action action = m_relevancyTracker.update(state, id, distance);
                
                if (action == RelevancyTracker::Action::Create) {
                    // Send Full Packet
                    continue;
                } else if (action == RelevancyTracker::Action::Destroy) {
                    // Send Destroy Packet
                    continue;
                }

                if (!state.relevantInstances.contains(id)) continue;
                if (!m_dirtyProperties.contains(id)) continue;

                float timeSinceLastSent = m_currentServerTime - m_lastSentTimes[clientId][id];
                float priority = m_priorityCalculator.calculate(distance, 0.0f, timeSinceLastSent); // 0.0 velocity mock
                prioritized.push_back({id, priority});
            }

            std::partial_sort(prioritized.begin(),
                prioritized.begin() + std::min((size_t)MAX_UPDATES_PER_CLIENT_PER_TICK, prioritized.size()),
                prioritized.end(), [](auto& a, auto& b) { return a.second > b.second; });

            for (int i = 0; i < std::min((int)MAX_UPDATES_PER_CLIENT_PER_TICK, (int)prioritized.size()); i++) {
                InstanceId id = prioritized[i].first;
                // Send partial packet...
                m_lastSentTimes[clientId][id] = m_currentServerTime;
            }
        }

        m_dirtyProperties.clear();
    }

}
