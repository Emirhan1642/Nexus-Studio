#include "ScriptScheduler.h"
#include "ScriptWatchdog.h"
#include <lualib.h>
#include <iostream>

namespace Engine::Scripting {

void ScriptScheduler::update(float deltaTime) {
    std::vector<SuspendedThread> readyThreads;

    for (auto it = m_suspendedThreads.begin(); it != m_suspendedThreads.end(); ) {
        it->remainingTime -= deltaTime;
        if (it->remainingTime <= 0.0) {
            readyThreads.push_back(*it);
            it = m_suspendedThreads.erase(it);
        } else {
            ++it;
        }
    }

    // Call lua_resume outside the iterator loop, because wait() can push new threads
    for (const auto& st : readyThreads) {
        ScriptExecutionContext ctx;
        ctx.scriptName = "Coroutine";
        ctx.startTime = std::chrono::steady_clock::now();
        ctx.phase = ScriptExecutionPhase::Heartbeat;
        ctx.budget = ScriptWatchdog::getBudgetFor(ctx.phase);
        ctx.instructionCount = 0;

        ScriptWatchdog::setCurrentContext(&ctx);
        int status = lua_resume(st.thread, nullptr, 0);
        ScriptWatchdog::setCurrentContext(nullptr);
        
        if (status != LUA_OK && status != LUA_YIELD) {
            std::cerr << "Luau Resume Error: " << lua_tostring(st.thread, -1) << std::endl;
        }
    }
}

void ScriptScheduler::suspendCurrentThread(lua_State* thread, double duration) {
    m_suspendedThreads.push_back({thread, duration});
}

int luau_wait(lua_State* L) {
    double duration = luaL_optnumber(L, 1, 0.0);
    ScriptScheduler::instance().suspendCurrentThread(L, duration);
    return lua_yield(L, 0);
}

} // namespace Engine::Scripting
