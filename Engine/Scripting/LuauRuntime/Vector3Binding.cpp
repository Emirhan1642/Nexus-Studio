#include "Vector3Binding.h"
#include <lualib.h>
#include <new>

namespace Engine::Scripting {

struct Vector3Userdata {
    float x, y, z;
};

static int vector3_index(lua_State* L) {
    Vector3Userdata* ud = static_cast<Vector3Userdata*>(lua_touserdatatagged(L, 1, kVector3Tag));
    const char* key = luaL_checkstring(L, 2);

    if (strcmp(key, "X") == 0) lua_pushnumber(L, ud->x);
    else if (strcmp(key, "Y") == 0) lua_pushnumber(L, ud->y);
    else if (strcmp(key, "Z") == 0) lua_pushnumber(L, ud->z);
    else {
        luaL_error(L, "'%s' is not a valid member of Vector3", key);
    }
    return 1;
}

static int vector3_newindex(lua_State* L) {
    luaL_error(L, "Vector3 cannot be modified. Create a new Vector3 instead.");
    return 0;
}

static int vector3_add(lua_State* L) {
    Vector3Userdata* a = static_cast<Vector3Userdata*>(lua_touserdatatagged(L, 1, kVector3Tag));
    Vector3Userdata* b = static_cast<Vector3Userdata*>(lua_touserdatatagged(L, 2, kVector3Tag));
    
    if (a && b) {
        pushVector3(L, {a->x + b->x, a->y + b->y, a->z + b->z});
        return 1;
    }
    luaL_error(L, "Attempt to add non-Vector3 to Vector3");
    return 0;
}

static int vector3_new(lua_State* L) {
    float x = (float)luaL_optnumber(L, 1, 0.0);
    float y = (float)luaL_optnumber(L, 2, 0.0);
    float z = (float)luaL_optnumber(L, 3, 0.0);
    pushVector3(L, {x, y, z});
    return 1;
}

void registerVector3Type(lua_State* L) {
    // Vector3 Metatable
    luaL_newmetatable(L, "Vector3Metatable");
    
    lua_pushcfunction(L, vector3_index, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, vector3_newindex, "__newindex");
    lua_setfield(L, -2, "__newindex");

    lua_pushcfunction(L, vector3_add, "__add");
    lua_setfield(L, -2, "__add");

    lua_pushstring(L, "Vector3");
    lua_setfield(L, -2, "__type");

    lua_pop(L, 1);

    // Vector3 global API (Vector3.new)
    lua_newtable(L);
    lua_pushcfunction(L, vector3_new, "new");
    lua_setfield(L, -2, "new");
    lua_setglobal(L, "Vector3");
}

void pushVector3(lua_State* L, const Engine::Math::Vector3& vec) {
    void* mem = lua_newuserdatatagged(L, sizeof(Vector3Userdata), kVector3Tag);
    new (mem) Vector3Userdata{vec.x, vec.y, vec.z};

    luaL_getmetatable(L, "Vector3Metatable");
    lua_setmetatable(L, -2);
}

Engine::Math::Vector3 checkVector3(lua_State* L, int index) {
    Vector3Userdata* ud = static_cast<Vector3Userdata*>(lua_touserdatatagged(L, index, kVector3Tag));
    if (!ud) {
        luaL_error(L, "Expected Vector3");
    }
    return {ud->x, ud->y, ud->z};
}

} // namespace Engine::Scripting
