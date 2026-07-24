#pragma once

#include <memory>
#include <string>
#include <set>
#include <any>
#include <vector>
#include "../../build/Engine/Networking/Messages.pb.h"
#include "../../Core/DataModel/Instance.h"

namespace Engine {
namespace Networking {

class PacketSerializer {
public:
    // Builds a replication packet containing the dirty properties for a given instance
    static Proto::ReplicationPacket buildReplicationPacket(const std::shared_ptr<Instance>& instance, const std::set<std::string>& dirtyProperties);
    
    // Applies a replication update to a given instance
    static void applyReplicationUpdate(const std::shared_ptr<Instance>& instance, const Proto::ReplicationUpdate& update);

    // Builds a RemoteEvent packet
    static Proto::RemoteEventPacket buildRemoteEventPacket(uint64_t instanceId, const std::vector<std::any>& args);

    // Deserializes RemoteEvent arguments
    static std::vector<std::any> deserializeRemoteEventArgs(const Proto::RemoteEventPacket& packet);
};

} // namespace Networking
} // namespace Engine
