#include "EnumRegistry.h"

namespace Engine::Reflection {

std::string EnumDescriptor::nameOf(int value) const {
    for (const auto& [name, v] : values) {
        if (v == value) return name;
    }
    return "Unknown";
}

int EnumDescriptor::valueOf(const std::string& name) const {
    for (const auto& [n, v] : values) {
        if (n == name) return v;
    }
    return -1;
}

EnumRegistry& EnumRegistry::instance() {
    static EnumRegistry reg;
    return reg;
}

EnumDescriptor& EnumRegistry::registerEnum(const std::string& name) {
    auto& desc = enums[name];
    desc.enumName = name;
    return desc;
}

EnumDescriptor* EnumRegistry::find(const std::string& name) {
    auto it = enums.find(name);
    return it != enums.end() ? &it->second : nullptr;
}

} // namespace Engine::Reflection
