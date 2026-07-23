#include "RemoteEvent.h"
#include <iostream>

void RemoteEvent::FireServer(const std::string& data) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Client) {
        std::cerr << "[RemoteEvent] FireServer can only be called from the Client.\n";
        return;
    }

    // Example payload construct
    // NetworkClient::instance().send(NetChannel::Reliable_Ordered, data.data(), data.size());
    std::cout << "[RemoteEvent] " << name << " firing Server with: " << data << "\n";
}

void RemoteEvent::FireClient(uint32_t clientId, const std::string& data) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Server) {
        std::cerr << "[RemoteEvent] FireClient can only be called from the Server.\n";
        return;
    }

    // NetworkServer::instance().sendTo(..., data);
    std::cout << "[RemoteEvent] " << name << " firing Client " << clientId << " with: " << data << "\n";
}

void RemoteEvent::FireAllClients(const std::string& data) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Server) {
        std::cerr << "[RemoteEvent] FireAllClients can only be called from the Server.\n";
        return;
    }

    // NetworkServer::instance().broadcast(NetChannel::Reliable_Ordered, data.data(), data.size());
    std::cout << "[RemoteEvent] " << name << " firing ALL Clients with: " << data << "\n";
}

void RemoteEvent::registerClass() {
    Engine::Reflection::ClassBuilder<RemoteEvent>("RemoteEvent")
        .base("Instance")
        .method("FireServer", &RemoteEvent::FireServer)
        .method("FireClient", &RemoteEvent::FireClient)
        .method("FireAllClients", &RemoteEvent::FireAllClients);
}
