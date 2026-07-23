#include "NetworkClient.h"
#include <iostream>

namespace Engine::Networking {

    NetworkClient& NetworkClient::instance() {
        static NetworkClient s_instance;
        return s_instance;
    }

    bool NetworkClient::connect(const std::string& hostName, uint16_t port) {
        if (enet_initialize() != 0) {
            std::cerr << "[NetworkClient] An error occurred while initializing ENet.\n";
            return false;
        }

        m_host = enet_host_create(nullptr, 1, static_cast<size_t>(NetChannel::ChannelCount), 0, 0);
        if (m_host == nullptr) {
            std::cerr << "[NetworkClient] An error occurred while trying to create an ENet client host.\n";
            return false;
        }

        ENetAddress address;
        enet_address_set_host(&address, hostName.c_str());
        address.port = port;

        m_peer = enet_host_connect(m_host, &address, static_cast<size_t>(NetChannel::ChannelCount), 0);
        if (m_peer == nullptr) {
            std::cerr << "[NetworkClient] No available peers for initiating an ENet connection.\n";
            return false;
        }

        ENetEvent event;
        // Wait up to 5 seconds for the connection attempt to succeed.
        if (enet_host_service(m_host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            std::cout << "[NetworkClient] Connection to " << hostName << ":" << port << " succeeded.\n";
            return true;
        } else {
            enet_peer_reset(m_peer);
            m_peer = nullptr;
            std::cerr << "[NetworkClient] Connection to " << hostName << ":" << port << " failed.\n";
            return false;
        }
    }

    void NetworkClient::disconnect() {
        if (m_peer) {
            enet_peer_disconnect(m_peer, 0);
            
            ENetEvent event;
            bool disconnected = false;
            while (enet_host_service(m_host, &event, 3000) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_RECEIVE:
                        enet_packet_destroy(event.packet);
                        break;
                    case ENET_EVENT_TYPE_DISCONNECT:
                        std::cout << "[NetworkClient] Disconnection succeeded.\n";
                        disconnected = true;
                        break;
                    default:
                        break;
                }
                if (disconnected) break;
            }

            if (!disconnected) {
                enet_peer_reset(m_peer);
            }
            m_peer = nullptr;
        }

        if (m_host) {
            enet_host_destroy(m_host);
            m_host = nullptr;
        }
        enet_deinitialize();
    }

    void NetworkClient::poll() {
        if (!m_host) return;

        ENetEvent event;
        while (enet_host_service(m_host, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE: {
                    if (m_packetHandler) {
                        m_packetHandler(event.packet->data, event.packet->dataLength, static_cast<NetChannel>(event.channelID));
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                case ENET_EVENT_TYPE_DISCONNECT: {
                    std::cout << "[NetworkClient] Server disconnected.\n";
                    m_peer = nullptr;
                    break;
                }
                default:
                    break;
            }
        }
    }

    void NetworkClient::send(NetChannel channel, const void* data, size_t length) {
        if (!m_peer) return;
        uint32_t flags = (channel == NetChannel::Reliable_Ordered) ? ENET_PACKET_FLAG_RELIABLE : 0;
        ENetPacket* packet = enet_packet_create(data, length, flags);
        enet_peer_send(m_peer, static_cast<enet_uint8>(channel), packet);
    }

}
