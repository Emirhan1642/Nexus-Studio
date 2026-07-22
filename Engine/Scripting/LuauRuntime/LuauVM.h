#pragma once
#include <lua.h>
#include <lualib.h>
#include <string>

class Script;

namespace Engine::Scripting {

class LuauVM {
public:
    static LuauVM& instance() {
        static LuauVM s_instance;
        return s_instance;
    }

    void init();
    void shutdown();

    lua_State* getState() const { return L; }

    // Kaynak kodu derler ve yeni bir thread içinde (coroutine) çalıştırır.
    bool executeScript(const std::string& source, Script* scriptInstance);

private:
    LuauVM() = default;
    ~LuauVM() = default;

    lua_State* L = nullptr;

    void removeUnsafeLibraries();
    void registerEngineAPI();
};

} // namespace Engine::Scripting
