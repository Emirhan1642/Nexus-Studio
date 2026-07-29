#pragma once

#include "Instance.h"
#include "../Math/Vector3.h"
#include "../../Animation/Skeleton.h"
#include <string>
#include <vector>

namespace Engine {

class IKControl : public Instance {
public:
    IKControl() = default;
    virtual ~IKControl() override = default;

    virtual std::string getClassName() const override { return "IKControl"; }

    // Properties
    std::string endEffector;
    Math::Vector3 targetPosition;
    Math::Vector3 poleVector = {0.0f, 0.0f, 1.0f};
    float weight = 1.0f;

    // Apply IK logic. Modifies localPose.
    void apply(Animation::Skeleton& skeleton, std::vector<Math::Matrix4>& localPose, const std::vector<Math::Matrix4>& worldPose);
};

} // namespace Engine
