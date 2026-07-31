#include "NetworkClient.h"
#include <iostream>
#include "../Serialization/PacketSerializer.h"
#include "../../Core/DataModel/InstanceRegistry.h"
#include "../../Core/DataModel/RemoteEvent.h"
#include "Messages.pb.h"
#include "../Prediction/HumanoidPredictor.h"

namespace Engine::Networking {

    NetworkClient& NetworkClient::instance() {
        static NetworkClient s_instance;
        return s_instance;
    }

    void NetworkClient::s_onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
        instance().onConnectionStatusChanged(pInfo);
    }

    bool NetworkClient::connect(const std::string& host, uint16_t port) {
        SteamNetworkingErrMsg errMsg;
        if (!GameNetworkingSockets_Init(nullptr, errMsg)) {
            std::cerr << "GameNetworkingSockets_Init failed: " << errMsg << std::endl;
            return false;
        }

        m_interface = SteamNetworkingSockets();
        if (!m_interface) return false;

        SteamNetworkingIPAddr serverAddr;
        serverAddr.Clear();
        serverAddr.ParseString(host.c_str());
        serverAddr.m_port = port;

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged, (void*)s_onConnectionStatusChanged);

        m_connection = m_interface->ConnectByIPAddress(serverAddr, 1, &opt);
        if (m_connection == k_HSteamNetConnection_Invalid) {
            std::cerr << "[Client] Failed to create connection" << std::endl;
            return false;
        }

        std::cout << "[Client] Connecting to " << host << ":" << port << "..." << std::endl;
        return true;
    }

    void NetworkClient::disconnect() {
        if (m_interface) {
            if (m_connection != k_HSteamNetConnection_Invalid) {
                m_interface->CloseConnection(m_connection, 0, "Disconnect", true);
                m_connection = k_HSteamNetConnection_Invalid;
            }
            // GameNetworkingSockets_Kill();
            m_interface = nullptr;
        }
    }

    void NetworkClient::onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo) {
        switch (pInfo->m_info.m_eState) {
            case k_ESteamNetworkingConnectionState_None:
                break;
            case k_ESteamNetworkingConnectionState_Connecting:
                break;
            case k_ESteamNetworkingConnectionState_FindingRoute:
                break;
            case k_ESteamNetworkingConnectionState_Connected:
                std::cout << "[Client] Connected to server successfully!" << std::endl;
                break;
            case k_ESteamNetworkingConnectionState_ClosedByPeer:
            case k_ESteamNetworkingConnectionState_ProblemDetectedLocally:
                std::cout << "[Client] Disconnected from server: " << pInfo->m_info.m_szEndDebug << std::endl;
                m_interface->CloseConnection(pInfo->m_hConn, 0, nullptr, false);
                m_connection = k_HSteamNetConnection_Invalid;
                break;
            default:
                break;
        }
    }

    void NetworkClient::poll() {
        if (!m_interface || m_connection == k_HSteamNetConnection_Invalid) return;

        m_interface->RunCallbacks();

        while (true) {
            ISteamNetworkingMessage* pIncomingMsg = nullptr;
            int numMsgs = m_interface->ReceiveMessagesOnConnection(m_connection, &pIncomingMsg, 1);
            if (numMsgs == 0) break;
            if (numMsgs < 0) {
                std::cerr << "[Client] Error checking for messages." << std::endl;
                break;
            }

            if (m_packetHandler) {
                NetChannel channel = NetChannel::Unreliable_State;
                m_packetHandler((const uint8_t*)pIncomingMsg->m_pData, pIncomingMsg->m_cbSize, channel);
            } else {
                Proto::NetworkPacket packet;
                if (packet.ParseFromArray(pIncomingMsg->m_pData, pIncomingMsg->m_cbSize)) {
                    if (packet.has_replication()) {
                        for (const auto& update : packet.replication().updates()) {
                            auto inst = InstanceRegistry::instance().findById(update.instance_id());
                            if (!inst) {
                                // Normally we would instantiate here via a generic factory. For now just lookup.
                                continue;
                            }
                            PacketSerializer::applyReplicationUpdate(inst, update);
                        }
                    } else if (packet.has_remote_event()) {
                        auto inst = InstanceRegistry::instance().findById(packet.remote_event().instance_id());
                        if (inst && inst->getClassName() == "RemoteEvent") {
                            auto re = std::static_pointer_cast<RemoteEvent>(inst);
                            std::vector<std::any> args = PacketSerializer::deserializeRemoteEventArgs(packet.remote_event());
                            re->triggerClientEvent(args);
                        }
                    } else if (packet.has_player_snapshot()) {
                        if (m_localPredictor) {
                            const auto& snap = packet.player_snapshot();
                            Math::Vector3 pos(snap.position().x(), snap.position().y(), snap.position().z());
                            Math::Vector3 vel(snap.velocity().x(), snap.velocity().y(), snap.velocity().z());
                            m_localPredictor->onServerSnapshot(snap.sequence_number(), pos, vel);
                        }
                    }
                }
            }

            pIncomingMsg->Release();
        }
    }

    void NetworkClient::send(NetChannel channel, const void* data, size_t length) {
        if (!m_interface || m_connection == k_HSteamNetConnection_Invalid) return;

        int flags = (channel == NetChannel::Reliable_Ordered) ? k_nSteamNetworkingSend_Reliable : k_nSteamNetworkingSend_Unreliable;
        m_interface->SendMessageToConnection(m_connection, data, length, flags, nullptr);
    }
}
