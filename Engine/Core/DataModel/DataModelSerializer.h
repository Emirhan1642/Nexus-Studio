#pragma once
#include "Instance.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <string>

namespace Engine::Core {

class DataModelSerializer {
public:
    // Serializes a given Instance and all its children into a JSON object
    static nlohmann::json serialize(const std::shared_ptr<Instance>& root);

    // Deserializes a JSON object into an Instance (creates it via TypeRegistry)
    static std::shared_ptr<Instance> deserialize(const nlohmann::json& j);
};

} // namespace Engine::Core
