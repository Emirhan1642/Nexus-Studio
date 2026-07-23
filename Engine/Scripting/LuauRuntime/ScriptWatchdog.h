#pragma once
#include <lua.h>
#include <chrono>
#include <string>

namespace Engine::Scripting {

    enum class ScriptExecutionPhase {
        Heartbeat,
        Initialization,
        RemoteEventCallback,
        EditorPluginCode
    };

    struct ExecutionBudget {
        int maxInstructions;
        std::chrono::milliseconds maxDuration;
    };

    struct ScriptExecutionContext {
        std::string scriptName;
        std::chrono::time_point<std::chrono::steady_clock> startTime;
        ExecutionBudget budget;
        int instructionCount = 0;
        ScriptExecutionPhase phase;
        bool warningShown = false;
    };

    class ScriptWatchdog {
    public:
        static void install(lua_State* L);
        
        static ExecutionBudget getBudgetFor(ScriptExecutionPhase phase);
        
        static void setCurrentContext(ScriptExecutionContext* ctx);
        static ScriptExecutionContext* getCurrentContext();

    private:
        static void onInterrupt(lua_State* L, int gc);
    };

} // namespace Engine::Scripting
