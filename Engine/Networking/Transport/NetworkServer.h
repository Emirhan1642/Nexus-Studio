#pragma once
#include <enet/enet.h>
#include <vector>
#include <functional>
#include <cstdint>
#include <string>

namespace Engine::Networking {

    enum class NetChannel : enet_uint8 {
        Reliable_Ordered = 0,
        Unreliable_State = 1,
        ChannelCount
    };

    struct ClientConnection {
        ENetPeer* peer = nullptr;
        uint32_t id = 0;
        // In the future: relevantInstances, playerCharacterPosition, pendingInitialSync, etc.
    };

    class NetworkServer {
    public:
        static NetworkServer& instance();

        bool start(uint16_t port, size_t maxClients = 32);
        void stop();
        void poll();

        void sendTo(ENetPeer* peer, NetChannel channel, const void* data, size_t length);
        void broadcast(NetChannel channel, const void* data, size_t length);

        using PacketHandler = std::function<void(ENetPeer*, const uint8_t*, size_t, NetChannel)>;
        void setPacketHandler(PacketHandler handler) { m_packetHandler = handler; }

    private:
        NetworkServer() = default;
        ~NetworkServer() { stop(); }

        ENetHost* m_host = nullptr;
        std::vector<ClientConnection> m_clients;
        uint32_t m_nextClientId = 1;

        PacketHandler m_packetHandler;
    };
}
