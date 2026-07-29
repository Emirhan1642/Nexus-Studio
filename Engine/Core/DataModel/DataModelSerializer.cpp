#include "DataModelSerializer.h"
#include "../Reflection/TypeRegistry.h"
#include "../Math/Vector3.h"

namespace Engine::Core {

std::string DataModelSerializer::getPath(const std::shared_ptr<Instance>& node, const std::shared_ptr<Instance>& root) {
    if (!node) return "";
    if (node == root) return node->name;
    std::string path = node->name;
    auto parent = node->getParent();
    while (parent && parent != root) {
        path = parent->name + "/" + path;
        parent = parent->getParent();
    }
    if (parent == root) {
        path = root->name + "/" + path;
    }
    return path;
}

std::shared_ptr<Instance> DataModelSerializer::resolvePath(const std::shared_ptr<Instance>& root, const std::string& path) {
    if (!root || path.empty()) return nullptr;
    
    std::vector<std::string> parts;
    size_t start = 0, end = 0;
    while ((end = path.find('/', start)) != std::string::npos) {
        parts.push_back(path.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(path.substr(start));

    auto current = root;
    size_t startIndex = 0;
    if (!parts.empty() && parts[0] == root->name) {
        startIndex = 1;
    }

    for (size_t i = startIndex; i < parts.size(); ++i) {
        if (!current) break;
        current = current->findFirstChild(parts[i]);
    }

    return current;
}

static nlohmann::json serializeValue(const Engine::Reflection::PropertyDescriptor& prop, const std::any& val) {
    if (prop.typeName == typeid(float).name()) return std::any_cast<float>(val);
    if (prop.typeName == typeid(int).name()) return std::any_cast<int>(val);
    if (prop.typeName == typeid(bool).name()) return std::any_cast<bool>(val);
    if (prop.typeName == typeid(std::string).name()) return std::any_cast<std::string>(val);
    if (prop.typeName == typeid(Engine::Math::Vector3).name()) {
        auto vec = std::any_cast<Engine::Math::Vector3>(val);
        return nlohmann::json::array({vec.x, vec.y, vec.z});
    }
    if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Enum) {
        return std::any_cast<int>(val);
    }
    return nullptr;
}

static std::any deserializeValue(const Engine::Reflection::PropertyDescriptor& prop, const nlohmann::json& jval) {
    if (prop.typeName == typeid(float).name()) return jval.get<float>();
    if (prop.typeName == typeid(int).name()) return jval.get<int>();
    if (prop.typeName == typeid(bool).name()) return jval.get<bool>();
    if (prop.typeName == typeid(std::string).name()) return jval.get<std::string>();
    if (prop.typeName == typeid(Engine::Math::Vector3).name()) {
        if (jval.is_array() && jval.size() == 3) {
            return Engine::Math::Vector3{jval[0].get<float>(), jval[1].get<float>(), jval[2].get<float>()};
        }
    }
    if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Enum) {
        return jval.get<int>();
    }
    return {};
}

nlohmann::json DataModelSerializer::serializeRecursive(const std::shared_ptr<Instance>& node, const std::shared_ptr<Instance>& root) {
    if (!node) return nlohmann::json();

    nlohmann::json j;
    j["className"] = node->getClassName();

    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(node->getClassName());
    if (classDesc) {
        nlohmann::json props = nlohmann::json::object();
        for (const auto& prop : classDesc->properties) {
            try {
                if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::ObjectRef) {
                    if (prop.objectGetter) {
                        auto target = prop.objectGetter(node.get());
                        if (target) {
                            props[prop.name] = getPath(target, root);
                        } else {
                            props[prop.name] = "";
                        }
                    }
                } else if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Array) {
                    if (prop.arraySize && prop.arrayGet) {
                        nlohmann::json arr = nlohmann::json::array();
                        size_t count = prop.arraySize(node.get());
                        for (size_t i = 0; i < count; ++i) {
                            std::any elem = prop.arrayGet(node.get(), i);
                            auto serialized = serializeValue(prop, elem);
                            if (!serialized.is_null()) arr.push_back(serialized);
                        }
                        props[prop.name] = arr;
                    }
                } else {
                    std::any val = prop.getter(node.get());
                    auto serialized = serializeValue(prop, val);
                    if (!serialized.is_null()) props[prop.name] = serialized;
                }
            } catch (...) {}
        }
        j["properties"] = props;
    }

    nlohmann::json childrenArray = nlohmann::json::array();
    for (const auto& child : node->getChildren()) {
        childrenArray.push_back(serializeRecursive(child, root));
    }
    j["children"] = childrenArray;

    return j;
}

nlohmann::json DataModelSerializer::serialize(const std::shared_ptr<Instance>& root) {
    return serializeRecursive(root, root);
}

std::shared_ptr<Instance> DataModelSerializer::deserializeRecursive(const nlohmann::json& j, std::vector<ObjectRefResolver>& resolvers) {
    if (!j.is_object() || !j.contains("className")) return nullptr;

    std::string className = j["className"];
    auto inst = createInstance(className);
    if (!inst) return nullptr;

    if (j.contains("properties") && j["properties"].is_object()) {
        auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(className);
        if (classDesc) {
            const auto& props = j["properties"];
            for (const auto& prop : classDesc->properties) {
                if (!props.contains(prop.name)) continue;
                
                const auto& jval = props[prop.name];
                
                try {
                    if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::ObjectRef) {
                        if (jval.is_string() && !jval.get<std::string>().empty()) {
                            resolvers.push_back({inst, prop.name, jval.get<std::string>()});
                        }
                    } else if (prop.kind == Engine::Reflection::PropertyDescriptor::Kind::Array) {
                        if (jval.is_array() && prop.arraySet) {
                            for (size_t i = 0; i < jval.size(); ++i) {
                                std::any deserialized = deserializeValue(prop, jval[i]);
                                if (deserialized.has_value()) {
                                    prop.arraySet(inst.get(), i, deserialized);
                                }
                            }
                        }
                    } else {
                        std::any deserialized = deserializeValue(prop, jval);
                        if (deserialized.has_value() && prop.setter) {
                            prop.setter(inst.get(), deserialized);
                        }
                    }
                } catch (...) {}
            }
        }
    }

    if (j.contains("children") && j["children"].is_array()) {
        for (const auto& childJson : j["children"]) {
            auto childInst = deserializeRecursive(childJson, resolvers);
            if (childInst) {
                childInst->setParent(inst);
            }
        }
    }

    return inst;
}

std::shared_ptr<Instance> DataModelSerializer::deserialize(const nlohmann::json& j) {
    std::vector<ObjectRefResolver> resolvers;
    auto root = deserializeRecursive(j, resolvers);
    
    if (root) {
        for (const auto& resolver : resolvers) {
            auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(resolver.owner->getClassName());
            if (classDesc) {
                const auto* prop = classDesc->findProperty(resolver.propertyName);
                if (prop && prop->objectSetter) {
                    auto target = resolvePath(root, resolver.targetPath);
                    if (target) {
                        prop->objectSetter(resolver.owner.get(), target);
                    }
                }
            }
        }
    }
    
    return root;
}

} // namespace Engine::Core
