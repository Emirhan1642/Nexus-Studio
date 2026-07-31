#pragma once

#include "../../Core/Math/Vector3.h"
#include "../../Core/DataModel/Humanoid.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace Engine {
namespace Networking {

struct HumanoidInputCommand {
    uint32_t sequenceNumber;
    float deltaTime;
    Math::Vector3 moveDirection;
    bool jumpRequested;
};

class HumanoidPredictor {
public:
    HumanoidPredictor(std::shared_ptr<Humanoid> humanoid);

    // Called every frame by the local client with player's input
    void onLocalInput(const Math::Vector3& moveDir, bool jump, float deltaTime);

    // Called when the server sends an authoritative snapshot for a specific sequence number
    void onServerSnapshot(uint32_t ackedSeq, const Math::Vector3& serverPos, const Math::Vector3& serverVelocity);

    size_t getPendingCommandsCount() const { return pendingCommands.size(); }

private:
    void sendToServer(const HumanoidInputCommand& cmd);

    std::shared_ptr<Humanoid> localHumanoid;
    std::vector<HumanoidInputCommand> pendingCommands;
    uint32_t currentSequence = 0;
};

} // namespace Networking
} // namespace Engine
