#include "HumanoidPredictor.h"
#include "../../Core/DataModel/Humanoid.h"
#include "../../Core/DataModel/Part.h"
#include "../../Physics/PhysicsWorld.h"
#include "../../Physics/PhysicsConversions.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

// Note: In a full network implementation, this would interact with NetworkClient.
// For now, we simulate the structure.
#include "../Transport/NetworkClient.h" 

namespace Engine {
namespace Networking {

HumanoidPredictor::HumanoidPredictor(std::shared_ptr<Humanoid> humanoid)
    : localHumanoid(humanoid) {
}

void HumanoidPredictor::onLocalInput(const Math::Vector3& moveDir, bool jump, float deltaTime) {
    if (!localHumanoid) return;

    HumanoidInputCommand cmd;
    cmd.sequenceNumber = ++currentSequence;
    cmd.deltaTime = deltaTime;
    cmd.moveDirection = moveDir;
    cmd.jumpRequested = jump;

    pendingCommands.push_back(cmd);

    // Predict immediately
    localHumanoid->applyMovement(cmd.moveDirection, cmd.deltaTime);
    if (cmd.jumpRequested) {
        localHumanoid->jump();
        // Since jump() just sets a flag, we need to process it right now in prediction context
        auto charVirt = localHumanoid->getCharacter();
        if (charVirt && charVirt->IsSupported()) {
            JPH::Vec3 vel = charVirt->GetLinearVelocity();
            vel.SetY(localHumanoid->jumpPower * 0.1f);
            charVirt->SetLinearVelocity(vel);
        }
    }

    sendToServer(cmd);
}

void HumanoidPredictor::onServerSnapshot(uint32_t ackedSeq, const Math::Vector3& serverPos, const Math::Vector3& serverVelocity) {
    if (!localHumanoid) return;

    // Erase all commands up to and including the acknowledged sequence
    pendingCommands.erase(
        std::remove_if(pendingCommands.begin(), pendingCommands.end(),
            [ackedSeq](const HumanoidInputCommand& c) { return c.sequenceNumber <= ackedSeq; }),
        pendingCommands.end()
    );

    auto charVirt = localHumanoid->getCharacter();
    if (!charVirt) return;

    // Hard reset to server state
    charVirt->SetPosition(Physics::toJoltVec3(serverPos));
    charVirt->SetLinearVelocity(Physics::toJoltVec3(serverVelocity));

    // Replay unacknowledged commands (Server reconciliation)
    for (const auto& cmd : pendingCommands) {
        localHumanoid->applyMovement(cmd.moveDirection, cmd.deltaTime);
        if (cmd.jumpRequested && charVirt->IsSupported()) {
            JPH::Vec3 vel = charVirt->GetLinearVelocity();
            vel.SetY(localHumanoid->jumpPower * 0.1f);
            charVirt->SetLinearVelocity(vel);
        }
    }
    
    // Sync the final replayed position back to the DataModel Part
    if (auto part = localHumanoid->getRootPart()) {
        part->setPosition(Physics::fromJoltVec3(charVirt->GetPosition()));
    }
}

void HumanoidPredictor::sendToServer(const HumanoidInputCommand& cmd) {
    // In actual implementation, we would serialize cmd to Protobuf/GNS and send via NetworkClient.
    // This is the placeholder for that network transmission layer.
}

} // namespace Networking
} // namespace Engine
