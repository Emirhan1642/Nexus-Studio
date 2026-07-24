#pragma once
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>
#include "../../Core/DataModel/Instance.h"

namespace Engine::Networking {

    enum class NetChannel : uint8_t {
        Reliable_Ordered = 0,
        Unreliable_State = 1,
        ChannelCount
    };

    struct ClientConnection {
        HSteamNetConnection connection = k_HSteamNetConnection_Invalid;
        uint32_t id = 0;
        InstanceId playerCharacter = 0;
        // In the future: pendingInitialSync, etc.
    };

    class NetworkServer {
    public:
        static NetworkServer& instance();

        bool start(uint16_t port, size_t maxClients = 32);
        void stop();
        void poll();

        void sendTo(HSteamNetConnection conn, NetChannel channel, const void* data, size_t length);
        void broadcast(NetChannel channel, const void* data, size_t length);

        using PacketHandler = std::function<void(HSteamNetConnection, const uint8_t*, size_t, NetChannel)>;
        void setPacketHandler(PacketHandler handler) { m_packetHandler = handler; }

        ISteamNetworkingSockets* getInterface() { return m_interface; }
        const std::vector<ClientConnection>& getClients() const { return m_clients; }

        void setPlayerCharacter(uint32_t clientId, InstanceId characterId);

    private:
        NetworkServer() = default;
        ~NetworkServer() { stop(); }

        void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
        static void s_onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);

        ISteamNetworkingSockets* m_interface = nullptr;
        HSteamListenSocket m_listenSocket = k_HSteamListenSocket_Invalid;
        HSteamNetPollGroup m_pollGroup = k_HSteamNetPollGroup_Invalid;

        std::vector<ClientConnection> m_clients;
        uint32_t m_nextClientId = 1;

        PacketHandler m_packetHandler;
    };
}
