#include "InstanceBinding.h"
#include "../../Core/DataModel/Instance.h"
#include "../../Core/Reflection/TypeRegistry.h"
#include "../../Core/Math/Vector3.h"
#include "Vector3Binding.h"
#include <lualib.h>
#include <iostream>

namespace Engine::Scripting {

struct InstanceUserdata {
    std::shared_ptr<Instance> instance;
};

struct SignalUserdata {
    Engine::Signal* signal;
};

static int signal_connect(lua_State* L) {
    SignalUserdata* ud = static_cast<SignalUserdata*>(lua_touserdatatagged(L, 1, kSignalTag));
    if (!ud || !ud->signal) {
        luaL_error(L, "Invalid Signal");
        return 0;
    }

    if (!lua_isfunction(L, 2)) {
        luaL_error(L, "Connect expects a function");
        return 0;
    }

    lua_pushvalue(L, 2);
    int funcRef = lua_ref(L, -1);
    lua_State* mainL = lua_mainthread(L);

    ud->signal->connect([mainL, funcRef](const std::vector<std::any>& args) {
        lua_rawgeti(mainL, LUA_REGISTRYINDEX, funcRef);
        for (const auto& arg : args) {
            pushAnyToLuau(mainL, arg);
        }
        if (lua_pcall(mainL, args.size(), 0, 0) != 0) {
            std::cerr << "Error in signal callback: " << lua_tostring(mainL, -1) << std::endl;
            lua_pop(mainL, 1);
        }
    });

    return 0;
}

static int signal_index(lua_State* L) {
    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "Connect") == 0) {
        lua_pushcfunction(L, signal_connect, "Connect");
        return 1;
    }
    return 0;
}

void registerSignalBinding(lua_State* L) {
    luaL_newmetatable(L, "SignalMetatable");
    lua_pushcfunction(L, signal_index, "__index");
    lua_setfield(L, -2, "__index");
    lua_pop(L, 1);
}

void pushSignal(lua_State* L, Engine::Signal* signal) {
    void* mem = lua_newuserdatatagged(L, sizeof(SignalUserdata), kSignalTag);
    new (mem) SignalUserdata{signal};
    luaL_getmetatable(L, "SignalMetatable");
    lua_setmetatable(L, -2);
}

// --- Generic Metamethods ---

static int instance_method_invoke(lua_State* L) {
    auto* methodDesc = static_cast<const Engine::Reflection::MethodDescriptor*>(lua_touserdata(L, lua_upvalueindex(1)));
    
    InstanceUserdata* ud = static_cast<InstanceUserdata*>(lua_touserdatatagged(L, 1, kInstanceTag));
    if (!ud || !ud->instance) {
        luaL_error(L, "Expected instance as first argument (use ':' instead of '.')");
        return 0;
    }

    int numArgs = lua_gettop(L);
    std::vector<std::any> args;
    for (int i = 2; i <= numArgs; ++i) {
        if (lua_isnumber(L, i)) {
            args.push_back((float)lua_tonumber(L, i));
        } else if (lua_isboolean(L, i)) {
            args.push_back((bool)lua_toboolean(L, i));
        } else if (lua_isstring(L, i)) {
            args.push_back(std::string(lua_tostring(L, i)));
        } else if (lua_touserdatatagged(L, i, kVector3Tag)) {
            args.push_back(checkVector3(L, i));
        } else if (InstanceUserdata* argUd = static_cast<InstanceUserdata*>(lua_touserdatatagged(L, i, kInstanceTag))) {
            args.push_back(argUd->instance);
        } else {
            args.push_back(std::any{}); // Fallback
        }
    }

    try {
        std::any result = methodDesc->invoke(ud->instance.get(), args);
        pushAnyToLuau(L, result);
        return result.has_value() ? 1 : 0;
    } catch (const std::exception& e) {
        luaL_error(L, "Method invocation error: %s", e.what());
        return 0;
    }
}

static int instance_index(lua_State* L) {
    InstanceUserdata* ud = static_cast<InstanceUserdata*>(lua_touserdatatagged(L, 1, kInstanceTag));
    if (!ud || !ud->instance) {
        luaL_error(L, "Invalid Instance");
        return 0;
    }

    const char* key = luaL_checkstring(L, 2);
    
    // Check Reflection Registry
    auto classDesc = Engine::Reflection::TypeRegistry::instance().find(ud->instance->getClassName());
    if (classDesc) {
        if (auto prop = classDesc->findProperty(key)) {
            std::any value = prop->getter(ud->instance.get());
            pushAnyToLuau(L, value);
            return 1;
        }
        if (auto sigDesc = classDesc->findSignal(key)) {
            Engine::Signal& sig = sigDesc->getSignal(ud->instance.get());
            pushSignal(L, &sig);
            return 1;
        }
        if (auto methodDesc = classDesc->findMethod(key)) {
            lua_pushlightuserdata(L, (void*)methodDesc);
            lua_pushcclosure(L, instance_method_invoke, methodDesc->name.c_str(), 1);
            return 1;
        }
    }

    luaL_error(L, "'%s' is not a valid member of %s", key, ud->instance->getClassName().c_str());
    return 0;
}

static int instance_newindex(lua_State* L) {
    InstanceUserdata* ud = static_cast<InstanceUserdata*>(lua_touserdatatagged(L, 1, kInstanceTag));
    if (!ud || !ud->instance) {
        luaL_error(L, "Invalid Instance");
        return 0;
    }

    const char* key = luaL_checkstring(L, 2);

    auto classDesc = Engine::Reflection::TypeRegistry::instance().find(ud->instance->getClassName());
    if (classDesc) {
        if (auto prop = classDesc->findProperty(key)) {
            if (prop->readOnly) {
                luaL_error(L, "'%s' is read-only", key);
                return 0;
            }

            try {
                std::any value = luauValueToAny(L, 3, prop->typeName);
                prop->setter(ud->instance.get(), value);
            } catch (const std::exception& e) {
                luaL_error(L, "Type mismatch for property '%s': %s", key, e.what());
            }
            return 0;
        }
    }

    luaL_error(L, "'%s' is not a valid member of %s", key, ud->instance->getClassName().c_str());
    return 0;
}

void registerInstanceBinding(lua_State* L) {
    luaL_newmetatable(L, "InstanceMetatable");
    
    lua_pushcfunction(L, instance_index, "__index");
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, instance_newindex, "__newindex");
    lua_setfield(L, -2, "__newindex");
    
    // Metatable name for typeof / tostring maybe?
    lua_pushstring(L, "Instance");
    lua_setfield(L, -2, "__type");

    lua_pop(L, 1);
}

void pushInstance(lua_State* L, const std::shared_ptr<Instance>& inst) {
    if (!inst) {
        lua_pushnil(L);
        return;
    }

    void* mem = lua_newuserdatatagged(L, sizeof(InstanceUserdata), kInstanceTag);
    new (mem) InstanceUserdata{inst};

    luaL_getmetatable(L, "InstanceMetatable");
    lua_setmetatable(L, -2);
}

void pushAnyToLuau(lua_State* L, const std::any& value) {
    if (value.type() == typeid(float)) {
        lua_pushnumber(L, std::any_cast<float>(value));
    } else if (value.type() == typeid(bool)) {
        lua_pushboolean(L, std::any_cast<bool>(value));
    } else if (value.type() == typeid(std::string)) {
        std::string s = std::any_cast<std::string>(value);
        lua_pushstring(L, s.c_str());
    } else if (value.type() == typeid(Engine::Math::Vector3)) {
        pushVector3(L, std::any_cast<Engine::Math::Vector3>(value));
    } else if (value.type() == typeid(std::shared_ptr<Instance>)) {
        pushInstance(L, std::any_cast<std::shared_ptr<Instance>>(value));
    } else {
        lua_pushnil(L);
    }
}

std::any luauValueToAny(lua_State* L, int index, const std::string& expectedType) {
    if (expectedType == typeid(float).name()) {
        return (float)luaL_checknumber(L, index);
    } else if (expectedType == typeid(bool).name()) {
        return (bool)lua_toboolean(L, index);
    } else if (expectedType == typeid(std::string).name()) {
        return std::string(luaL_checkstring(L, index));
    } else if (expectedType == typeid(Engine::Math::Vector3).name()) {
        return checkVector3(L, index);
    } else {
        throw std::runtime_error("Unsupported type");
    }
}

} // namespace Engine::Scripting
