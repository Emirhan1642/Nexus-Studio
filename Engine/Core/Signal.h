#pragma once

#include <functional>
#include <vector>
#include <any>
#include <algorithm>
#include <cstdint>

namespace Engine {

class Signal {
public:
    struct Connection {
        uint32_t id;
        std::function<void(const std::vector<std::any>&)> callback;
    };

    uint32_t connect(std::function<void(const std::vector<std::any>&)> cb) {
        uint32_t id = nextId++;
        connections.push_back({id, std::move(cb)});
        return id;
    }

    void disconnect(uint32_t id) {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [id](const Connection& c) { return c.id == id; }),
            connections.end()
        );
    }

    void fire(const std::vector<std::any>& args) {
        for (auto& conn : connections) {
            conn.callback(args);
        }
    }

private:
    std::vector<Connection> connections;
    uint32_t nextId = 1;
};

} // namespace Engine
