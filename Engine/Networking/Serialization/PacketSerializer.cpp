#include "PacketSerializer.h"
#include "../../Core/Reflection/TypeRegistry.h"
#include "../../Core/Math/Vector3.h"

namespace Engine {
namespace Networking {

Proto::ReplicationPacket PacketSerializer::buildReplicationPacket(const std::shared_ptr<Instance>& instance, const std::set<std::string>& dirtyProperties) {
    Proto::ReplicationPacket packet;
    auto* update = packet.add_updates();
    update->set_instance_id(instance->getInstanceId());
    update->set_class_name(instance->getClassName());

    auto* classDesc = Reflection::TypeRegistry::instance().find(instance->getClassName());
    if (!classDesc) return packet;

    for (const auto& propName : dirtyProperties) {
        auto* prop = classDesc->findProperty(propName);
        if (!prop || !prop->replicated) continue;

        std::any value = prop->getter(instance.get());
        Proto::PropertyData propData;

        if (value.type() == typeid(int)) {
            propData.set_int_value(std::any_cast<int>(value));
        } else if (value.type() == typeid(float)) {
            propData.set_float_value(std::any_cast<float>(value));
        } else if (value.type() == typeid(std::string)) {
            propData.set_string_value(std::any_cast<std::string>(value));
        } else if (value.type() == typeid(bool)) {
            propData.set_bool_value(std::any_cast<bool>(value));
        } else if (value.type() == typeid(Math::Vector3)) {
            auto vec = std::any_cast<Math::Vector3>(value);
            Proto::Vector3* pVec = propData.mutable_vector3_value();
            pVec->set_x(vec.x);
            pVec->set_y(vec.y);
            pVec->set_z(vec.z);
        } else {
            // Ignore unsupported types for now
            continue;
        }

        (*update->mutable_properties())[propName] = propData;
    }

    return packet;
}

void PacketSerializer::applyReplicationUpdate(const std::shared_ptr<Instance>& instance, const Proto::ReplicationUpdate& update) {
    auto* classDesc = Reflection::TypeRegistry::instance().find(instance->getClassName());
    if (!classDesc) return;

    for (const auto& [propName, propData] : update.properties()) {
        auto* prop = classDesc->findProperty(propName);
        if (!prop || !prop->replicated) continue;

        std::any value;
        switch (propData.value_case()) {
            case Proto::PropertyData::kIntValue:
                value = propData.int_value();
                break;
            case Proto::PropertyData::kFloatValue:
                value = propData.float_value();
                break;
            case Proto::PropertyData::kStringValue:
                value = propData.string_value();
                break;
            case Proto::PropertyData::kBoolValue:
                value = propData.bool_value();
                break;
            case Proto::PropertyData::kVector3Value: {
                const auto& vec = propData.vector3_value();
                value = Math::Vector3(vec.x(), vec.y(), vec.z());
                break;
            }
            default:
                continue;
        }

        prop->setter(instance.get(), value);
    }
}

Proto::RemoteEventPacket PacketSerializer::buildRemoteEventPacket(uint64_t instanceId, const std::vector<std::any>& args) {
    Proto::RemoteEventPacket packet;
    packet.set_instance_id(instanceId);

    for (const auto& arg : args) {
        Proto::PropertyData propData;
        if (arg.type() == typeid(int)) {
            propData.set_int_value(std::any_cast<int>(arg));
        } else if (arg.type() == typeid(float)) {
            propData.set_float_value(std::any_cast<float>(arg));
        } else if (arg.type() == typeid(std::string)) {
            propData.set_string_value(std::any_cast<std::string>(arg));
        } else if (arg.type() == typeid(bool)) {
            propData.set_bool_value(std::any_cast<bool>(arg));
        } else if (arg.type() == typeid(Math::Vector3)) {
            auto vec = std::any_cast<Math::Vector3>(arg);
            Proto::Vector3* pVec = propData.mutable_vector3_value();
            pVec->set_x(vec.x);
            pVec->set_y(vec.y);
            pVec->set_z(vec.z);
        } else {
            // Unhandled arg type
        }
        *packet.add_args() = propData;
    }

    return packet;
}

std::vector<std::any> PacketSerializer::deserializeRemoteEventArgs(const Proto::RemoteEventPacket& packet) {
    std::vector<std::any> args;
    for (const auto& propData : packet.args()) {
        std::any value;
        switch (propData.value_case()) {
            case Proto::PropertyData::kIntValue:
                value = propData.int_value();
                break;
            case Proto::PropertyData::kFloatValue:
                value = propData.float_value();
                break;
            case Proto::PropertyData::kStringValue:
                value = propData.string_value();
                break;
            case Proto::PropertyData::kBoolValue:
                value = propData.bool_value();
                break;
            case Proto::PropertyData::kVector3Value: {
                const auto& vec = propData.vector3_value();
                value = Math::Vector3(vec.x(), vec.y(), vec.z());
                break;
            }
            default:
                break;
        }
        args.push_back(value);
    }
    return args;
}

} // namespace Networking
} // namespace Engine
