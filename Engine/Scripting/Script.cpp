#include "Script.h"
#include "LuauRuntime/LuauVM.h"
#include "../Core/Reflection/ClassBuilder.h"
#include <iostream>

void Script::onAddedToWorkspace() {
    Instance::onAddedToWorkspace();
    
    // Script'i LuauVM üzerinden çalıştırıyoruz.
    if (!m_hasRun) {
        m_hasRun = true;
        bool success = Engine::Scripting::LuauVM::instance().executeScript(source, this);
        if (!success) {
            std::cerr << "Script failed to execute: " << name << std::endl;
        }
    }
}

void Script::onRemovedFromWorkspace() {
    Instance::onRemovedFromWorkspace();
    // Script durdurulabilir veya connection'lar temizlenebilir (gelişmiş safhalarda).
}

struct ScriptReflectionInit {
    ScriptReflectionInit() {
        using namespace Engine::Reflection;
        ClassBuilder<Script>("Script")
            .base("Instance")
            .property("Source", &Script::source).category("Scripting");
    }
} g_scriptReflectionInit;
