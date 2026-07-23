#include "LuauVM.h"
#include "ScriptScheduler.h"
#include "InstanceBinding.h"
#include "Vector3Binding.h"
#include "ScriptWatchdog.h"
#include <Luau/Compiler.h>
#include <iostream>

namespace Engine::Scripting {

static int safe_os_time(lua_State* L) {
    // Sadece zaman döndüren zararsız bir fonksiyon
    lua_pushnumber(L, (double)time(nullptr));
    return 1;
}

void LuauVM::init() {
    L = lua_newstate([](void*, void* ptr, size_t osize, size_t nsize) -> void* {
        if (nsize == 0) {
            free(ptr);
            return nullptr;
        }
        return realloc(ptr, nsize);
    }, nullptr);

    luaL_openlibs(L); // Temel kütüphaneler (string, table, math)
    removeUnsafeLibraries();
    registerEngineAPI();
    
    // Register Bindings
    registerVector3Type(L);
    registerSignalBinding(L);
    registerInstanceBinding(L);
    
    ScriptWatchdog::install(L);
}

void LuauVM::shutdown() {
    if (L) {
        lua_close(L);
        L = nullptr;
    }
}

void LuauVM::removeUnsafeLibraries() {
    // io, os, debug, package gibi kütüphaneleri kaldır.
    // Roblox tarzı izolasyon.
    lua_pushnil(L); lua_setglobal(L, "io");
    lua_pushnil(L); lua_setglobal(L, "package");
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");
    lua_pushnil(L); lua_setglobal(L, "require");
    
    // os kütüphanesini sadece zararsız fonksiyonlarla değiştir
    lua_newtable(L);
    lua_pushcfunction(L, safe_os_time, "time");
    lua_setfield(L, -2, "time");
    lua_setglobal(L, "os");
}

void LuauVM::registerEngineAPI() {
    // Legacy global wait
    lua_pushcfunction(L, luau_wait, "wait");
    lua_setglobal(L, "wait");

    // Task API
    lua_newtable(L);
    lua_pushcfunction(L, luau_task_wait, "wait");
    lua_setfield(L, -2, "wait");
    lua_pushcfunction(L, luau_task_spawn, "spawn");
    lua_setfield(L, -2, "spawn");
    lua_pushcfunction(L, luau_task_delay, "delay");
    lua_setfield(L, -2, "delay");
    lua_setglobal(L, "task");
}

bool LuauVM::executeScript(const std::string& source, Script* scriptInstance) {
    // 1. Compile
    Luau::CompileOptions options;
    options.optimizationLevel = 1;
    options.debugLevel = 1;

    std::string bytecode = Luau::compile(source, options);
    if (bytecode.empty() || bytecode[0] == '\0') {
        std::cerr << "Luau Compile Error." << std::endl;
        return false; // Compile failed
    }

    // 2. Sandboxing (Her script kendi thread'inde ve ortamında çalışır)
    lua_State* thread = lua_newthread(L);

    // Boş bir environment oluştur
    lua_newtable(thread);
    lua_newtable(thread);
    lua_pushvalue(thread, LUA_GLOBALSINDEX);
    lua_setfield(thread, -2, "__index"); // Fallback to global API
    lua_setmetatable(thread, -2);
    
    // Yükle
    std::string chunkName = "Script"; // scriptInstance->name
    if (luau_load(thread, chunkName.c_str(), bytecode.data(), bytecode.size(), 0) != 0) {
        std::cerr << "Luau Load Error: " << lua_tostring(thread, -1) << std::endl;
        return false;
    }

    // Set fenv of the loaded function
    lua_pushvalue(thread, -2); // The environment table we created
    lua_setfenv(thread, -2);

    // 3. Resume
    ScriptExecutionContext ctx;
    ctx.scriptName = chunkName;
    ctx.startTime = std::chrono::steady_clock::now();
    ctx.phase = ScriptExecutionPhase::Initialization;
    ctx.budget = ScriptWatchdog::getBudgetFor(ctx.phase);
    ctx.instructionCount = 0;
    
    ScriptWatchdog::setCurrentContext(&ctx);
    int status = lua_resume(thread, L, 0);
    ScriptWatchdog::setCurrentContext(nullptr);
    
    if (status != LUA_OK && status != LUA_YIELD) {
        std::cerr << "Luau Runtime Error: " << lua_tostring(thread, -1) << std::endl;
        return false;
    }

    return true;
}

} // namespace Engine::Scripting
