#pragma once
#include "Instance.h"
#include "../../../Engine/Networking/Transport/NetworkServer.h"
#include "../../../Engine/Networking/Transport/NetworkClient.h"
#include "../../../Engine/Networking/Transport/NetworkContext.h"
#include <string>

class RemoteEvent : public Instance {
public:
    std::string getClassName() const override { return "RemoteEvent"; }

    // Called from Luau on Client
    void FireServer(const std::string& data);

    // Called from Luau on Server
    void FireClient(uint32_t clientId, const std::string& data);

    // Called from Luau on Server to all clients
    void FireAllClients(const std::string& data);

    static void registerClass();
};
