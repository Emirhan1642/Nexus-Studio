#include "TypeRegistry.h"

namespace Engine::Reflection {

TypeRegistry& TypeRegistry::instance() {
    static TypeRegistry reg;
    return reg;
}

ClassDescriptor& TypeRegistry::registerClass(const std::string& name) {
    auto& desc = classes[name];
    desc.className = name;
    return desc;
}

void TypeRegistry::deferBaseClass(const std::string& className, const std::string& baseName) {
    pending.push_back({className, baseName});
}

void TypeRegistry::finalize() {
    for (const auto& p : pending) {
        ClassDescriptor* derived = find(p.className);
        ClassDescriptor* base = find(p.baseClassName);

        if (!base) {
            throw std::runtime_error(
                "Reflection error: Base class '" + p.baseClassName + 
                "' for class '" + p.className + "' not found. "
                "Is it a typo, or is the base class not registered?"
            );
        }
        derived->baseClass = base;
    }
    pending.clear();
}

ClassDescriptor* TypeRegistry::find(const std::string& name) {
    auto it = classes.find(name);
    return it != classes.end() ? &it->second : nullptr;
}

} // namespace Engine::Reflection
