#include "NetworkServer.h"
#include <iostream>
#include <cassert>
#include "../Serialization/PacketSerializer.h"
#include "../../Core/DataModel/InstanceRegistry.h"
#include "../../Core/DataModel/RemoteEvent.h"
#include "../../build/Engine/Networking/Messages.pb.h"

namespace Engine::Networking {

    NetworkServer& NetworkServer::instance() {
        static NetworkServer s_instance;
        return s_instance;
    }

    void NetworkServer::s_onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
        instance().onConnectionStatusChanged(pInfo);
    }

    bool NetworkServer::start(uint16_t port, size_t maxClients) {
        SteamNetworkingErrMsg errMsg;
        if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
            std::cerr << "GameNetworkingSockets_Init failed: " << errMsg << std::endl;
            return false;
        }

        m_interface = SteamNetworkingSockets();
        if (!m_interface) return false;

        SteamNetworkingIPAddr serverLocalAddr;
        serverLocalAddr.Clear();
        serverLocalAddr.m_port = port;

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)s_onConnectionStatusChanged);

        m_listenSocket = m_interface->CreateListenSocketIP(serverLocalAddr, 1, &opt);
        if (m_listenSocket == k_HSteamListenSocket_Invalid) {
            std::cerr << "Failed to listen on port " << port << std::endl;
            return false;
        }

        m_pollGroup = m_interface->CreatePollGroup();
        if (m_pollGroup == k_HSteamNetPollGroup_Invalid) {
            std::cerr << "Failed to create poll group" << std::endl;
            return false;
        }

        std::cout << "[Server] Listening on port " << port << std::endl;
        return true;
    }

    void NetworkServer::stop() {
        if (m_interface) {
            if (m_listenSocket != k_HSteamListenSocket_Invalid) {
                m_interface->CloseListenSocket(m_listenSocket);
                m_listenSocket = k_HSteamListenSocket_Invalid;
            }
            if (m_pollGroup != k_HSteamNetPollGroup_Invalid) {
                m_interface->DestroyPollGroup(m_pollGroup);
                m_pollGroup = k_HSteamNetPollGroup_Invalid;
            }
            // Close all connections
            for (auto& client : m_clients) {
                m_interface->CloseConnection(client.connection, 0, "Server Shutdown", true);
            }
            m_clients.clear();
            // GameNetworkingSockets_Kill();
            m_interface = nullptr;
        }
    }

    void NetworkServer::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
        switch (pInfo->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_None:
                break;
            case k_ESteamNetworkingConnectionState_Connecting:
                // Accept new connection
                if (m_interface->AcceptConnection(pInfo->m_hConn) != k_EResultOK) {
                    m_interface->CloseConnection(pInfo->m_hConn, 0, "Failed to accept", false);
                    std::cerr << "[Server] Failed to accept connection." << std::endl;
                    break;
                }
                if (!m_interface->SetConnectionPollGroup(pInfo->m_hConn, m_pollGroup)) {
                    m_interface->CloseConnection(pInfo->m_hConn, 0, "Failed to set poll group", false);
                    std::cerr << "[Server] Failed to set poll group." << std::endl;
                    break;
                }
                std::cout << "[Server] Accepted connection " << pInfo->m_hConn << std::endl;
                break;
            case k_ESteamNetworkingConnectionState_FindingRoute:
                break;
            case k_ESteamNetworkingConnectionState_Connected: {
                ClientConnection client;
                client.connection = pInfo->m_hConn;
                client.id = m_nextClientId++;
                m_clients.push_back(client);
                std::cout << "[Server] Client " << client.id << " fully connected." << std::endl;
                break;
            }
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally: {
                // Client disconnected
                for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
                    if (it->connection == pInfo->m_hConn) {
                        std::cout << "[Server] Client " << it->id << " disconnected: " << pInfo->m_info.m_szEndDebug << std::endl;
                        m_clients.erase(it);
                        break;
                    }
                }
                m_interface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                break;
            }
            default:
                break;
        }
    }

    void NetworkServer::poll() {
        if (!m_interface) return;

        m_interface->RunCallbacks();

        while (true) {
            ISteamNetworkingMessage* pIncomingMsg = nullptr;
            int numMsgs = m_interface->ReceiveMessagesOnPollGroup(m_pollGroup, &pIncomingMsg, 1);
            if (numMsgs == 0) break;
            if (numMsgs < 0) {
                std::cerr << "[Server] Error checking for messages." << std::endl;
                break;
            }

            if (m_packetHandler) {
                NetChannel channel = NetChannel::Unreliable_State; // Default to unreliable for GNS unless custom struct passed
                m_packetHandler(pIncomingMsg->m_conn, (const uint8_t*)pIncomingMsg->m_pData, pIncomingMsg->m_cbSize, channel);
            } else {
                Proto::NetworkPacket packet;
                if (packet.ParseFromArray(pIncomingMsg->m_pData, pIncomingMsg->m_cbSize)) {
                    if (packet.has_remote_event()) {
                        auto inst = InstanceRegistry::instance().findById(packet.remote_event().instance_id());
                        if (inst && inst->getClassName() == "RemoteEvent") {
                            auto re = std::static_pointer_cast<RemoteEvent>(inst);
                            std::vector<std::any> args = PacketSerializer::deserializeRemoteEventArgs(packet.remote_event());
                            re->triggerServerEvent(args);
                        }
                    }
                }
            }

            pIncomingMsg->Release();
        }
    }

    void NetworkServer::sendTo(HSteamNetConnection conn, NetChannel channel, const void* data, size_t length) {
        if (!m_interface) return;
        int flags = (channel == NetChannel::Reliable_Ordered) ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
        m_interface->SendMessageToConnection(conn, data, length, flags, nullptr);
    }

    void NetworkServer::broadcast(NetChannel channel, const void* data, size_t length) {
        if (!m_interface) return;
        int flags = (channel == NetChannel::Reliable_Ordered) ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
        for (const auto& client : m_clients) {
            m_interface->SendMessageToConnection(client.connection, data, length, flags, nullptr);
        }
    }
}
