#pragma once
namespace Engine::Networking {
    enum class NetworkMode {
        Standalone,
        Server,
        Client
    };

    class NetworkContext {
    public:
        static NetworkMode mode() { return s_mode; }
        static void setMode(NetworkMode mode) { s_mode = mode; }
    private:
        static NetworkMode s_mode;
    };
}
