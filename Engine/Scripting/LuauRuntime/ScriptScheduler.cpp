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

void ScriptScheduler::spawnThread(lua_State* thread) {
    m_suspendedThreads.push_back({thread, 0.0});
}

void ScriptScheduler::delayThread(lua_State* thread, double duration) {
    m_suspendedThreads.push_back({thread, duration});
}

int luau_wait(lua_State* L) {
    double duration = luaL_optnumber(L, 1, 0.0);
    ScriptScheduler::instance().suspendCurrentThread(L, duration);
    return lua_yield(L, 0);
}

int luau_task_wait(lua_State* L) {
    double duration = luaL_optnumber(L, 1, 0.0);
    ScriptScheduler::instance().suspendCurrentThread(L, duration);
    return lua_yield(L, 0);
}

int luau_task_spawn(lua_State* L) {
    if (!lua_isfunction(L, 1)) {
        luaL_error(L, "task.spawn expects a function");
        return 0;
    }
    
    // Create a new coroutine
    lua_State* thread = lua_newthread(L);
    lua_pushvalue(L, 1); // Copy function
    lua_xmove(L, thread, 1); // Move function to new thread

    // Move any additional arguments
    int numArgs = lua_gettop(L) - 2; // -1 for thread, -1 for function
    if (numArgs > 0) {
        lua_xmove(L, thread, numArgs);
    }
    
    ScriptScheduler::instance().spawnThread(thread);
    return 1; // Return the thread
}

int luau_task_delay(lua_State* L) {
    double duration = luaL_checknumber(L, 1);
    if (!lua_isfunction(L, 2)) {
        luaL_error(L, "task.delay expects a function");
        return 0;
    }

    // Create a new coroutine
    lua_State* thread = lua_newthread(L);
    lua_pushvalue(L, 2); // Copy function
    lua_xmove(L, thread, 1); // Move function to new thread

    // Move any additional arguments
    int numArgs = lua_gettop(L) - 3; // -1 for thread, -1 for function, -1 for duration
    if (numArgs > 0) {
        lua_xmove(L, thread, numArgs);
    }

    ScriptScheduler::instance().delayThread(thread, duration);
    return 1; // Return the thread
}

} // namespace Engine::Scripting
