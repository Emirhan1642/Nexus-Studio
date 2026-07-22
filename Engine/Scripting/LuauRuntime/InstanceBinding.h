#pragma once
#include <lua.h>
#include <string>
#include <memory>
#include <any>

class Instance;

namespace Engine::Scripting {

// Constants for userdata tags (Luau specific)
constexpr int kInstanceTag = 1;

void registerInstanceBinding(lua_State* L);

// C++ nesnesini Luau'ya gönderir
void pushInstance(lua_State* L, const std::shared_ptr<Instance>& inst);

// C++'tan gelen std::any değerini Luau'ya dönüştürür
void pushAnyToLuau(lua_State* L, const std::any& value);

// Luau değerini C++ std::any'e dönüştürür (Tip kontrolü yaparak)
std::any luauValueToAny(lua_State* L, int index, const std::string& expectedType);

} // namespace Engine::Scripting
