#include "ScriptWatchdog.h"
#include <lualib.h>

namespace Engine::Scripting {

    static thread_local ScriptExecutionContext* s_currentContext = nullptr;

    ExecutionBudget ScriptWatchdog::getBudgetFor(ScriptExecutionPhase phase) {
        switch (phase) {
            case ScriptExecutionPhase::Heartbeat:
                return {500000, std::chrono::milliseconds(8)};
            case ScriptExecutionPhase::Initialization:
                return {5000000, std::chrono::milliseconds(1000)};
            case ScriptExecutionPhase::RemoteEventCallback:
                return {1000000, std::chrono::milliseconds(50)};
            case ScriptExecutionPhase::EditorPluginCode:
                return {50000000, std::chrono::milliseconds(5000)};
            default:
                return {500000, std::chrono::milliseconds(8)};
        }
    }

    void ScriptWatchdog::setCurrentContext(ScriptExecutionContext* ctx) {
        s_currentContext = ctx;
    }

    ScriptExecutionContext* ScriptWatchdog::getCurrentContext() {
        return s_currentContext;
    }

    void ScriptWatchdog::install(lua_State* L) {
        lua_Callbacks* callbacks = lua_callbacks(L);
        callbacks->interrupt = onInterrupt;
    }

    void ScriptWatchdog::onInterrupt(lua_State* L, int gc) {
        if (gc >= 0) return; // gc >= 0 indicates a GC interrupt. We only care about instruction interrupts (which usually pass -1 or use other signatures in Luau depending on the version, but in standard Luau `gc >= 0` means GC).

        ScriptExecutionContext* ctx = getCurrentContext();
        if (!ctx) return;

        // Luau interrupts every certain number of instructions (e.g. 100k or 1M, or loop edges depending on the compiler).
        // For simplicity, we just add a nominal value, but we primarily rely on the wall-clock time.
        ctx->instructionCount += 100000;

        bool overInstructionBudget = ctx->instructionCount > ctx->budget.maxInstructions;
        auto elapsed = std::chrono::steady_clock::now() - ctx->startTime;
        bool overTimeBudget = elapsed > ctx->budget.maxDuration;

        if (overInstructionBudget || overTimeBudget) {
            luaL_error(L, "Script execution budget exceeded (%s) in '%s'",
                overTimeBudget ? "time limit" : "instruction limit",
                ctx->scriptName.c_str());
        }
    }

} // namespace Engine::Scripting
