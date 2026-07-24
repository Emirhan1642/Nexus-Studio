#include "RemoteEvent.h"
#include <iostream>
#include "../../Networking/Serialization/PacketSerializer.h"

void RemoteEvent::FireServer(const std::vector<std::any>& args) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Client) {
        std::cerr << "[RemoteEvent] FireServer can only be called from the Client.\n";
        return;
    }

    auto rePacket = Engine::Networking::PacketSerializer::buildRemoteEventPacket(getInstanceId(), args);
    Engine::Networking::Proto::NetworkPacket masterPacket;
    *masterPacket.mutable_remote_event() = rePacket;
    std::string outData;
    masterPacket.SerializeToString(&outData);

    Engine::Networking::NetworkClient::instance().send(Engine::Networking::NetChannel::Reliable_Ordered, outData.data(), outData.size());
}

void RemoteEvent::FireClient(uint32_t clientId, const std::vector<std::any>& args) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Server) {
        std::cerr << "[RemoteEvent] FireClient can only be called from the Server.\n";
        return;
    }

    auto clients = Engine::Networking::NetworkServer::instance().getClients();
    auto it = std::find_if(clients.begin(), clients.end(), [clientId](const Engine::Networking::ClientConnection& c) { return c.id == clientId; });
    if (it != clients.end()) {
        auto rePacket = Engine::Networking::PacketSerializer::buildRemoteEventPacket(getInstanceId(), args);
        Engine::Networking::Proto::NetworkPacket masterPacket;
        *masterPacket.mutable_remote_event() = rePacket;
        std::string outData;
        masterPacket.SerializeToString(&outData);
        Engine::Networking::NetworkServer::instance().sendTo(it->connection, Engine::Networking::NetChannel::Reliable_Ordered, outData.data(), outData.size());
    }
}

void RemoteEvent::FireAllClients(const std::vector<std::any>& args) {
    if (Engine::Networking::NetworkContext::mode() != Engine::Networking::NetworkMode::Server) {
        std::cerr << "[RemoteEvent] FireAllClients can only be called from the Server.\n";
        return;
    }

    auto rePacket = Engine::Networking::PacketSerializer::buildRemoteEventPacket(getInstanceId(), args);
    Engine::Networking::Proto::NetworkPacket masterPacket;
    *masterPacket.mutable_remote_event() = rePacket;
    std::string outData;
    masterPacket.SerializeToString(&outData);

    Engine::Networking::NetworkServer::instance().broadcast(Engine::Networking::NetChannel::Reliable_Ordered, outData.data(), outData.size());
}

void RemoteEvent::registerClass() {
    Engine::Reflection::ClassBuilder<RemoteEvent>("RemoteEvent")
        .base("Instance")
        .method("FireServer", &RemoteEvent::FireServer)
        .method("FireClient", &RemoteEvent::FireClient)
        .method("FireAllClients", &RemoteEvent::FireAllClients);
}
