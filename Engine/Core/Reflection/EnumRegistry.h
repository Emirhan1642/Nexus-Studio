#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine::Reflection {

struct EnumDescriptor {
    std::string enumName;
    std::vector<std::pair<std::string, int>> values;

    std::string nameOf(int value) const;
    int valueOf(const std::string& name) const;
};

class EnumRegistry {
public:
    static EnumRegistry& instance();

    EnumDescriptor& registerEnum(const std::string& name);
    EnumDescriptor* find(const std::string& name);

private:
    EnumRegistry() = default;
    
    std::unordered_map<std::string, EnumDescriptor> enums;
};

} // namespace Engine::Reflection
