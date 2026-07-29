#pragma once
#include "Instance.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <vector>

namespace Engine::Core {

struct ObjectRefResolver {
    std::shared_ptr<Instance> owner;
    std::string propertyName;
    std::string targetPath;
};

class DataModelSerializer {
public:
    static nlohmann::json serialize(const std::shared_ptr<Instance>& root);
    static std::shared_ptr<Instance> deserialize(const nlohmann::json& j);

private:
    static nlohmann::json serializeRecursive(const std::shared_ptr<Instance>& node, const std::shared_ptr<Instance>& root);
    static std::shared_ptr<Instance> deserializeRecursive(const nlohmann::json& j, std::vector<ObjectRefResolver>& resolvers);
    static std::string getPath(const std::shared_ptr<Instance>& node, const std::shared_ptr<Instance>& root);
    static std::shared_ptr<Instance> resolvePath(const std::shared_ptr<Instance>& root, const std::string& path);
};

} // namespace Engine::Core

