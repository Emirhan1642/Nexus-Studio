#pragma once
#include <lua.h>
#include <vector>
#include <chrono>

namespace Engine::Scripting {

class ServerFrameBudgetGuard {
public:
    void beginFrame() { totalScriptTimeThisFrame = std::chrono::milliseconds(0); }

    bool hasRemainingBudget() const {
        return totalScriptTimeThisFrame < MAX_TOTAL_SCRIPT_TIME_PER_FRAME;
    }

    void recordScriptTime(std::chrono::milliseconds elapsed) {
        totalScriptTimeThisFrame += elapsed;
    }

private:
    std::chrono::milliseconds totalScriptTimeThisFrame{0};
    // 10ms frame budget for scripts
    static constexpr auto MAX_TOTAL_SCRIPT_TIME_PER_FRAME = std::chrono::milliseconds(10);
};

class ScriptScheduler {
public:
    static ScriptScheduler& instance() {
        static ScriptScheduler s_instance;
        return s_instance;
    }

    void update(float deltaTime);

    void suspendCurrentThread(lua_State* thread, double duration);
    void spawnThread(lua_State* thread);
    void delayThread(lua_State* thread, double duration);

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
// Global 'wait'
int luau_wait(lua_State* L);

// Global 'task' library functions
int luau_task_wait(lua_State* L);
int luau_task_spawn(lua_State* L);
int luau_task_delay(lua_State* L);

} // namespace Engine::Scripting
