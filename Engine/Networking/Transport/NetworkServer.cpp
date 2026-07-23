#include "NetworkServer.h"
#include <iostream>

namespace Engine::Networking {

    NetworkServer& NetworkServer::instance() {
        static NetworkServer s_instance;
        return s_instance;
    }

    bool NetworkServer::start(uint16_t port, size_t maxClients) {
        if (enet_initialize() != 0) {
            std::cerr << "[NetworkServer] An error occurred while initializing ENet.\n";
            return false;
        }

        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;

        m_host = enet_host_create(&address, maxClients, static_cast<size_t>(NetChannel::ChannelCount), 0, 0);
        if (m_host == nullptr) {
            std::cerr << "[NetworkServer] An error occurred while trying to create an ENet server host.\n";
            return false;
        }

        std::cout << "[NetworkServer] Started on port " << port << "\n";
        return true;
    }

    void NetworkServer::stop() {
        if (m_host) {
            enet_host_destroy(m_host);
            m_host = nullptr;
            enet_deinitialize();
            std::cout << "[NetworkServer] Stopped.\n";
        }
    }

    void NetworkServer::poll() {
        if (!m_host) return;

        ENetEvent event;
        while (enet_host_service(m_host, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: {
                    std::cout << "[NetworkServer] A new client connected from " 
                              << event.peer->address.host << ":" << event.peer->address.port << "\n";
                    ClientConnection conn;
                    conn.peer = event.peer;
                    conn.id = m_nextClientId++;
                    event.peer->data = reinterpret_cast<void*>(static_cast<uintptr_t>(conn.id));
                    m_clients.push_back(conn);
                    break;
                }
                case ENET_EVENT_TYPE_RECEIVE: {
                    if (m_packetHandler) {
                        m_packetHandler(event.peer, event.packet->data, event.packet->dataLength, static_cast<NetChannel>(event.channelID));
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    std::cout << "[NetworkServer] Client disconnected.\n";
                    uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(event.peer->data));
                    event.peer->data = nullptr;
                    // Remove from m_clients
                    std::erase_if(m_clients, [id](const ClientConnection& c) { return c.id == id; });
                    break;
                }
                case ENET_EVENT_TYPE_NONE:
                    break;
            }
        }
    }

    void NetworkServer::sendTo(ENetPeer* peer, NetChannel channel, const void* data, size_t length) {
        if (!peer) return;
        uint32_t flags = (channel == NetChannel::Reliable_Ordered) ? ENET_PACKET_FLAG_RELIABLE : 0;
        ENetPacket* packet = enet_packet_create(data, length, flags);
        enet_peer_send(peer, static_cast<enet_uint8>(channel), packet);
    }

    void NetworkServer::broadcast(NetChannel channel, const void* data, size_t length) {
        if (!m_host) return;
        uint32_t flags = (channel == NetChannel::Reliable_Ordered) ? ENET_PACKET_FLAG_RELIABLE : 0;
        ENetPacket* packet = enet_packet_create(data, length, flags);
        enet_host_broadcast(m_host, static_cast<enet_uint8>(channel), packet);
    }

}
