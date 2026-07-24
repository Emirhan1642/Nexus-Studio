#pragma once
#include "Instance.h"
#include "../../../Engine/Networking/Transport/NetworkServer.h"
#include "../../../Engine/Networking/Transport/NetworkClient.h"
#include "../../../Engine/Networking/Transport/NetworkContext.h"
#include <string>
#include <vector>
#include <any>
#include <functional>

class RemoteEvent : public Instance {
public:
    std::string getClassName() const override { return "RemoteEvent"; }

    // Mock Signal typedef since we don't have a full Signal library here yet
    using SignalCallback = std::function<void(const std::vector<std::any>&)>;
    std::vector<SignalCallback> onServerEventCallbacks;
    std::vector<SignalCallback> onClientEventCallbacks;

    void connectServerEvent(SignalCallback cb) { onServerEventCallbacks.push_back(cb); }
    void connectClientEvent(SignalCallback cb) { onClientEventCallbacks.push_back(cb); }

    void triggerServerEvent(const std::vector<std::any>& args) {
        for (auto& cb : onServerEventCallbacks) cb(args);
    }
    void triggerClientEvent(const std::vector<std::any>& args) {
        for (auto& cb : onClientEventCallbacks) cb(args);
    }

    // Called from Luau on Client
    void FireServer(const std::vector<std::any>& args);

    // Called from Luau on Server
    void FireClient(uint32_t clientId, const std::vector<std::any>& args);

    // Called from Luau on Server to all clients
    void FireAllClients(const std::vector<std::any>& args);

    static void registerClass();
};
