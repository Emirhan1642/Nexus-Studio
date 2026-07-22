#pragma once
#include <lua.h>
#include "../../Core/Math/Vector3.h"

namespace Engine::Scripting {

constexpr int kVector3Tag = 2;

void registerVector3Type(lua_State* L);

// C++ Vector3'ü Luau userdata olarak stack'e ekler
void pushVector3(lua_State* L, const Engine::Math::Vector3& vec);

// Luau stack'indeki değeri C++ Vector3 olarak okur
Engine::Math::Vector3 checkVector3(lua_State* L, int index);

} // namespace Engine::Scripting
