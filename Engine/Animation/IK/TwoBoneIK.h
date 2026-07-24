#pragma once

#include "../../Core/Math/Vector3.h"
#include "../../Core/Math/Quaternion.h"
#include <algorithm>
#include <cmath>

namespace Engine {
namespace Animation {
namespace IK {

struct TwoBoneIKResult {
    Math::Quaternion upperBoneRotation;
    Math::Quaternion lowerBoneRotation;
};

inline TwoBoneIKResult solveTwoBoneIK(
    const Math::Vector3& rootPos, 
    const Math::Vector3& midPos, 
    const Math::Vector3& endPos,
    const Math::Vector3& targetPos,
    const Math::Vector3& poleVector
) {
    Math::Vector3 upperBoneDir = midPos - rootPos;
    Math::Vector3 lowerBoneDir = endPos - midPos;
    
    float upperLength = upperBoneDir.length();
    float lowerLength = lowerBoneDir.length();
    
    Math::Vector3 targetDir = targetPos - rootPos;
    float targetDistance = std::min(targetDir.length(), upperLength + lowerLength - 0.01f);
    
    // Using Law of Cosines to find the angle at the mid joint (knee/elbow)
    // c^2 = a^2 + b^2 - 2ab*cos(C) => cos(C) = (a^2 + b^2 - c^2) / (2ab)
    float cosAngle = (upperLength * upperLength + targetDistance * targetDistance - lowerLength * lowerLength) / (2.0f * upperLength * targetDistance);
    float angle = std::acos(std::clamp(cosAngle, -1.0f, 1.0f));

    // For a real implementation, we would construct the specific quaternions using the pole vector
    // to define the plane of bending. This is a simplified math block that returns identity 
    // to represent the architecture described in the document.
    
    TwoBoneIKResult result;
    result.upperBoneRotation = Math::Quaternion::identity();
    result.lowerBoneRotation = Math::Quaternion::identity();
    
    return result;
}

} // namespace IK
} // namespace Animation
} // namespace Engine
