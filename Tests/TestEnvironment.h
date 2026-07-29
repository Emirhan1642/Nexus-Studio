#pragma once
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Reflection/TypeRegistry.h"

inline void ensureTestEnvironmentInitialized() {
    static bool s_initialized = false;
    if (!s_initialized) {
        Engine::Physics::PhysicsWorld::initJolt();
        Engine::Physics::PhysicsWorld::instance().initialize();
        Engine::Reflection::TypeRegistry::instance().finalize();
        s_initialized = true;
    }
}
