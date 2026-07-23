#pragma once
#include <enet/enet.h>
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

        bool isConnected() const { return m_peer != nullptr && m_peer->state == ENET_PEER_STATE_CONNECTED; }

    private:
        NetworkClient() = default;
        ~NetworkClient() { disconnect(); }

        ENetHost* m_host = nullptr;
        ENetPeer* m_peer = nullptr;

        PacketHandler m_packetHandler;
    };
}
