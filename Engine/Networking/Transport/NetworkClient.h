#pragma once
#include <steam/steamnetworkingsockets.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#include <string>
#include <functional>

#include "NetworkServer.h" // For NetChannel enum

namespace Engine::Networking {

    class NetworkClient {
    public:
        static NetworkClient& instance();

        bool connect(const std::string& host, uint16_t port);
        void disconnect();
        void poll();

        void send(NetChannel channel, const void* data, size_t length);

        using PacketHandler = std::function<void(const uint8_t*, size_t, NetChannel)>;
        void setPacketHandler(PacketHandler handler) { m_packetHandler = handler; }

    private:
        NetworkClient() = default;
        ~NetworkClient() { disconnect(); }

        void onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);
        static void s_onConnectionStatusChanged(SteamNetConnectionStatusChangedCallback_t* pInfo);

        ISteamNetworkingSockets* m_interface = nullptr;
        HSteamNetConnection m_connection = k_HSteamNetConnection_Invalid;

        PacketHandler m_packetHandler;
    };
}
