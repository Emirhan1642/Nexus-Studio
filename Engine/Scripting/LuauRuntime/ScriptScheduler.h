#pragma once
#include <lua.h>
#include <vector>

namespace Engine::Scripting {

class ScriptScheduler {
public:
    static ScriptScheduler& instance() {
        static ScriptScheduler s_instance;
        return s_instance;
    }

    void update(float deltaTime);
    void suspendCurrentThread(lua_State* thread, double duration);

private:
    ScriptScheduler() = default;
    ~ScriptScheduler() = default;

    struct SuspendedThread {
        lua_State* thread;
        double remainingTime;
    };

    std::vector<SuspendedThread> m_suspendedThreads;
};

// Global luau wait function
int luau_wait(lua_State* L);

} // namespace Engine::Scripting
