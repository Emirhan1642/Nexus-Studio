#define NOMINMAX
#include "ReplicationManager.h"
#include <iostream>
#include "../Serialization/PacketSerializer.h"
#include "../../Core/DataModel/InstanceRegistry.h"

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

        // 0. Update SpatialGrid for all instances that have a Position
        auto allInstances = InstanceRegistry::instance().getAllInstances();
        for (const auto& inst : allInstances) {
            auto typeDesc = Reflection::TypeRegistry::instance().find(inst->getClassName());
            if (typeDesc) {
                if (auto prop = typeDesc->findProperty("Position")) {
                    std::any val = prop->getter(inst.get());
                    if (val.type() == typeid(Math::Vector3)) {
                        Math::Vector3 pos = std::any_cast<Math::Vector3>(val);
                        m_spatialGrid.updateInstancePosition(inst->getInstanceId(), pos);
                    }
                }
            }
        }

        m_dormancyManager.updateDormancy(m_currentServerTime, m_spatialGrid);

        // Iterate over connected clients from NetworkServer
        auto clients = NetworkServer::instance().getClients();
        for (const auto& clientConn : clients) {
            uint32_t clientId = clientConn.id;
            ClientRelevancyState& state = m_clientStates[clientId];

            // 1. Initial Sync Throttling
            int sentInitial = 0;
            while (!state.pendingInitialSync.empty() && sentInitial < INITIAL_SYNC_BATCH_SIZE) {
                InstanceId id = state.pendingInitialSync.back();
                state.pendingInitialSync.pop_back();
                
                auto inst = InstanceRegistry::instance().findById(id);
                if (inst) {
                    std::set<std::string> allProps; // Need all props for initial sync. Will simplify by sending a mock full sync
                    auto classDesc = Reflection::TypeRegistry::instance().find(inst->getClassName());
                    if (classDesc) {
                        for (auto& prop : classDesc->properties) {
                            if (prop.replicated) allProps.insert(prop.name);
                        }
                    }
                    auto repPacket = PacketSerializer::buildReplicationPacket(inst, allProps);
                    Proto::NetworkPacket masterPacket;
                    *masterPacket.mutable_replication() = repPacket;
                    std::string outData;
                    masterPacket.SerializeToString(&outData);

                    // Find connection for client
                    auto clients = NetworkServer::instance().getClients();
                    auto it = std::find_if(clients.begin(), clients.end(), [clientId](const ClientConnection& c) { return c.id == clientId; });
                    if (it != clients.end()) {
                        NetworkServer::instance().sendTo(it->connection, NetChannel::Reliable_Ordered, outData.data(), outData.size());
                        
                        std::cout << "[ReplicationManager] Sent initial sync for ID: " << id << " to Client: " << clientId << std::endl;
                        
                        m_lastSentTimes[clientId][id] = m_currentServerTime;
                    }
                }
                sentInitial++;
            }

            // 2. Always Relevant Objects
            for (InstanceId id : AlwaysRelevantRegistry::instance().getAll()) {
                if (m_dirtyProperties.contains(id)) {
                    auto inst = InstanceRegistry::instance().findById(id);
                    if (inst) {
                        std::set<std::string> dirtyProps(m_dirtyProperties[id].begin(), m_dirtyProperties[id].end());
                        auto repPacket = PacketSerializer::buildReplicationPacket(inst, dirtyProps);
                        Proto::NetworkPacket masterPacket;
                        *masterPacket.mutable_replication() = repPacket;
                        std::string outData;
                        masterPacket.SerializeToString(&outData);

                        auto clients = NetworkServer::instance().getClients();
                        auto it = std::find_if(clients.begin(), clients.end(), [clientId](const ClientConnection& c) { return c.id == clientId; });
                        if (it != clients.end()) {
                            NetworkServer::instance().sendTo(it->connection, NetChannel::Reliable_Ordered, outData.data(), outData.size());
                        }
                    }
                }
            }

            // 3. Spatial Grid & Priority calculation
            Math::Vector3 playerPos(0.0f, 0.0f, 0.0f);
            
            if (clientConn.playerCharacter != 0) {
                auto charInst = InstanceRegistry::instance().findById(clientConn.playerCharacter);
                if (charInst) {
                    auto typeDesc = Reflection::TypeRegistry::instance().find(charInst->getClassName());
                    if (typeDesc) {
                        if (auto prop = typeDesc->findProperty("Position")) {
                            std::any val = prop->getter(charInst.get());
                            if (val.type() == typeid(Math::Vector3)) {
                                playerPos = std::any_cast<Math::Vector3>(val);
                            }
                        }
                    }
                }
            }
            
            std::vector<InstanceId> nearby = m_spatialGrid.queryRadius(playerPos, 5); // 500 studs (5 cells)
            
            std::vector<std::pair<InstanceId, float>> prioritized;
            for (InstanceId id : nearby) {
                if (m_dormancyManager.isDormant(id)) continue;
                
                // distance check
                float distance = 0.0f;
                auto inst = InstanceRegistry::instance().findById(id);
                if (inst) {
                    auto typeDesc = Reflection::TypeRegistry::instance().find(inst->getClassName());
                    if (typeDesc) {
                        if (auto prop = typeDesc->findProperty("Position")) {
                            std::any val = prop->getter(inst.get());
                            if (val.type() == typeid(Math::Vector3)) {
                                Math::Vector3 pos = std::any_cast<Math::Vector3>(val);
                                distance = (pos - playerPos).length();
                            }
                        }
                    }
                }
                
                RelevancyTracker::Action action = m_relevancyTracker.update(state, id, distance);
                
                if (action == RelevancyTracker::Action::Create) {
                    std::cout << "[ReplicationManager] Action::Create queued for ID: " << id << std::endl;
                    // Add to pending initial sync for next tick
                    if (std::find(state.pendingInitialSync.begin(), state.pendingInitialSync.end(), id) == state.pendingInitialSync.end()) {
                        state.pendingInitialSync.push_back(id);
                    }
                    continue;
                } else if (action == RelevancyTracker::Action::Destroy) {
                    // Send Destroy Packet (Not fully implemented in MVP, just skipping)
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
                
                auto inst = InstanceRegistry::instance().findById(id);
                if (inst) {
                    std::set<std::string> dirtyProps(m_dirtyProperties[id].begin(), m_dirtyProperties[id].end());
                    auto repPacket = PacketSerializer::buildReplicationPacket(inst, dirtyProps);
                    
                    Proto::NetworkPacket masterPacket;
                    *masterPacket.mutable_replication() = repPacket;
                    std::string outData;
                    masterPacket.SerializeToString(&outData);

                    auto clients = NetworkServer::instance().getClients();
                    auto it = std::find_if(clients.begin(), clients.end(), [clientId](const ClientConnection& c) { return c.id == clientId; });
                    if (it != clients.end()) {
                        NetworkServer::instance().sendTo(it->connection, NetChannel::Unreliable_State, outData.data(), outData.size());
                    }
                }
                
                m_lastSentTimes[clientId][id] = m_currentServerTime;
            }
        }

        m_dirtyProperties.clear();
    }

}
