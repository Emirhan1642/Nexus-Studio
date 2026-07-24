#include "DataModelSerializer.h"
#include "../Reflection/TypeRegistry.h"
#include "../Math/Vector3.h"

namespace Engine::Core {

nlohmann::json DataModelSerializer::serialize(const std::shared_ptr<Instance>& root) {
    if (!root) return nlohmann::json();

    nlohmann::json j;
    j["className"] = root->getClassName();

    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(root->getClassName());
    if (classDesc) {
        nlohmann::json props = nlohmann::json::object();
        for (const auto& prop : classDesc->properties) {
            // For MVP, skip ObjectRef and Array
            if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::ObjectRef ||
                prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Array) {
                continue;
            }

            try {
                std::any val = prop.getter(root.get());

                if (prop.typeName == typeid(float).name()) {
                    props[prop.name] = std::any_cast<float>(val);
                } else if (prop.typeName == typeid(int).name()) {
                    props[prop.name] = std::any_cast<int>(val);
                } else if (prop.typeName == typeid(bool).name()) {
                    props[prop.name] = std::any_cast<bool>(val);
                } else if (prop.typeName == typeid(std::string).name()) {
                    props[prop.name] = std::any_cast<std::string>(val);
                } else if (prop.typeName == typeid(Engine::Math::Vector3).name()) {
                    auto vec = std::any_cast<Engine::Math::Vector3>(val);
                    props[prop.name] = {vec.x, vec.y, vec.z};
                } else if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Enum) {
                    props[prop.name] = std::any_cast<int>(val); // Enums are cast to int in ClassBuilder
                }
            } catch (...) {
                // Ignore any_cast failures
            }
        }
        j["properties"] = props;
    }

    nlohmann::json childrenArray = nlohmann::json::array();
    for (const auto& child : root->getChildren()) {
        childrenArray.push_back(serialize(child));
    }
    j["children"] = childrenArray;

    return j;
}

std::shared_ptr<Instance> DataModelSerializer::deserialize(const nlohmann::json& j) {
    if (!j.is_object() || !j.contains("className")) return nullptr;

    std::string className = j["className"];
    auto inst = createInstance(className);
    if (!inst) return nullptr;

    if (j.contains("properties") && j["properties"].is_object()) {
        auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(className);
        if (classDesc) {
            const auto& props = j["properties"];
            for (const auto& prop : classDesc->properties) {
                if (props.contains(prop.name)) {
                    try {
                        if (prop.typeName == typeid(float).name()) {
                            prop.setter(inst.get(), props[prop.name].get<float>());
                        } else if (prop.typeName == typeid(int).name()) {
                            prop.setter(inst.get(), props[prop.name].get<int>());
                        } else if (prop.typeName == typeid(bool).name()) {
                            prop.setter(inst.get(), props[prop.name].get<bool>());
                        } else if (prop.typeName == typeid(std::string).name()) {
                            prop.setter(inst.get(), props[prop.name].get<std::string>());
                        } else if (prop.typeName == typeid(Engine::Math::Vector3).name()) {
                            auto arr = props[prop.name];
                            if (arr.is_array() && arr.size() == 3) {
                                Engine::Math::Vector3 vec{arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>()};
                                prop.setter(inst.get(), vec);
                            }
                        } else if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Enum) {
                            prop.setter(inst.get(), props[prop.name].get<int>());
                        }
                    } catch (...) {
                        // Ignore parse or setter failures
                    }
                }
            }
        }
    }

    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& childJson : j["children"]) {
            auto childInst = deserialize(childJson);
            if (childInst) {
                childInst->setParent(inst);
            }
        }
    }

    return inst;
}

} // namespace Engine::Core
