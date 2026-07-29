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
    float targetDistance = targetDir.length();
    if (targetDistance > upperLength + lowerLength - 0.001f) {
        targetDir.normalize();
        targetDistance = upperLength + lowerLength - 0.001f;
    }
    
    // Calculate bend axis
    Math::Vector3 bendAxis = targetDir.cross(poleVector - rootPos);
    if (bendAxis.length() < 0.001f) {
        bendAxis = upperBoneDir.cross(lowerBoneDir);
        if (bendAxis.length() < 0.001f) bendAxis = Math::Vector3(1, 0, 0); // fallback
    }
    bendAxis.normalize();
    
    // Angles using Law of Cosines
    float cosRoot = (upperLength * upperLength + targetDistance * targetDistance - lowerLength * lowerLength) / (2.0f * upperLength * targetDistance);
    float rootAngle = std::acos(std::clamp(cosRoot, -1.0f, 1.0f));
    
    float cosMid = (upperLength * upperLength + lowerLength * lowerLength - targetDistance * targetDistance) / (2.0f * upperLength * lowerLength);
    float midAngle = std::acos(std::clamp(cosMid, -1.0f, 1.0f));
    
    // Create base rotation to aim at target
    upperBoneDir.normalize();
    targetDir.normalize();
    Math::Vector3 rotAxis = upperBoneDir.cross(targetDir);
    float rotAngle = std::acos(std::clamp(upperBoneDir.dot(targetDir), -1.0f, 1.0f));
    if (rotAxis.length() > 0.001f) rotAxis.normalize();
    else rotAxis = Math::Vector3(0, 1, 0);
    
    Math::Quaternion aimRot = Math::Quaternion::fromAxisAngle(rotAxis, rotAngle);
    Math::Quaternion bendRot = Math::Quaternion::fromAxisAngle(bendAxis, rootAngle);
    
    TwoBoneIKResult result;
    // In a full implementation, these quaternions would be composed with the bone's local/world 
    // coordinate system. This represents the delta rotations.
    result.upperBoneRotation = bendRot; 
    result.lowerBoneRotation = Math::Quaternion::fromAxisAngle(Math::Vector3(0,0,1), midAngle - 3.14159f);
    
    return result;
}

} // namespace IK
} // namespace Animation
} // namespace Engine
