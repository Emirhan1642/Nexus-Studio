#include "IKControl.h"
#include "../Reflection/TypeRegistry.h"
#include "../../Animation/IK/TwoBoneIK.h"

namespace Engine {

void IKControl::apply(Animation::Skeleton& skeleton, std::vector<Math::Matrix4>& localPose, const std::vector<Math::Matrix4>& worldPose) {
    if (weight <= 0.0f) return;

    int endIndex = skeleton.findBoneIndex(endEffector);
    if (endIndex == -1) return;

    int midIndex = skeleton.bones[endIndex].parentIndex;
    if (midIndex == -1) return;

    int rootIndex = skeleton.bones[midIndex].parentIndex;
    if (rootIndex == -1) return;

    Math::Vector3 rootPos = worldPose[rootIndex].getTranslation();
    Math::Vector3 midPos = worldPose[midIndex].getTranslation();
    Math::Vector3 endPos = worldPose[endIndex].getTranslation();

    // Prevent over-stretching
    Math::Vector3 targetDir = targetPosition - rootPos;
    float maxDist = (midPos - rootPos).length() + (endPos - midPos).length() - 0.001f;
    Math::Vector3 target = targetPosition;
    if (targetDir.length() > maxDist) {
        targetDir.normalize();
        target = rootPos + targetDir * maxDist;
    }

    // CCD IK (2 iterations are usually enough for 2 bones)
    for (int iter = 0; iter < 2; ++iter) {
        // --- Mid Bone ---
        Math::Vector3 midWorld = worldPose[midIndex].getTranslation();
        Math::Vector3 endWorld = worldPose[endIndex].getTranslation();
        Math::Vector3 midToEnd = endWorld - midWorld;
        Math::Vector3 midToTarget = target - midWorld;
        
        midToEnd.normalize();
        midToTarget.normalize();
        
        float dotMid = std::clamp(midToEnd.dot(midToTarget), -1.0f, 1.0f);
        if (dotMid < 0.9999f) {
            float angleMid = std::acos(dotMid);
            Math::Vector3 axisMid = midToEnd.cross(midToTarget);
            if (axisMid.length() > 0.001f) {
                axisMid.normalize();
                Math::Quaternion rotWorld = Math::Quaternion::fromAxisAngle(axisMid, angleMid);
                
                // Convert world rotation to local space of the mid bone parent (root)
                Math::Vector3 rootTrans, rootScale;
                Math::Quaternion rootRot;
                worldPose[rootIndex].decompose(rootTrans, rootRot, rootScale);
                
                Math::Quaternion localRotDelta = rootRot.inverse() * rotWorld * rootRot;
                
                // Apply to mid bone local pose
                Math::Vector3 mTrans, mScale;
                Math::Quaternion mRot;
                localPose[midIndex].decompose(mTrans, mRot, mScale);
                
                Math::Quaternion finalRot = localRotDelta * mRot;
                // Blend by weight
                mRot = mRot.slerp(finalRot, weight);
                localPose[midIndex] = Math::Matrix4::fromTRS(mTrans, mRot, mScale);
                
                // Update world pose for next bone calculation
                // Note: In a full CCD, we re-evaluate world transforms here.
                // For a 2-bone chain, we can approximate the end effector's new position.
                // But since we have computeWorldTransforms, we can just do a partial or full recompute.
            }
        }
        
        // Recompute world pose for the chain to get new end position for Root bone step
        std::vector<Math::Matrix4> tempWorld = skeleton.computeWorldTransforms(localPose);
        endWorld = tempWorld[endIndex].getTranslation();
        
        // --- Root Bone ---
        Math::Vector3 rootWorld = tempWorld[rootIndex].getTranslation();
        Math::Vector3 rootToEnd = endWorld - rootWorld;
        Math::Vector3 rootToTarget = target - rootWorld;
        
        rootToEnd.normalize();
        rootToTarget.normalize();
        
        float dotRoot = std::clamp(rootToEnd.dot(rootToTarget), -1.0f, 1.0f);
        if (dotRoot < 0.9999f) {
            float angleRoot = std::acos(dotRoot);
            Math::Vector3 axisRoot = rootToEnd.cross(rootToTarget);
            if (axisRoot.length() > 0.001f) {
                axisRoot.normalize();
                Math::Quaternion rotWorld = Math::Quaternion::fromAxisAngle(axisRoot, angleRoot);
                
                // Convert world rotation to local space of the root bone parent
                int rootParentIndex = skeleton.bones[rootIndex].parentIndex;
                Math::Quaternion parentRot = Math::Quaternion::identity();
                if (rootParentIndex != -1) {
                    Math::Vector3 pTrans, pScale;
                    tempWorld[rootParentIndex].decompose(pTrans, parentRot, pScale);
                }
                
                Math::Quaternion localRotDelta = parentRot.inverse() * rotWorld * parentRot;
                
                // Apply to root bone local pose
                Math::Vector3 rTrans, rScale;
                Math::Quaternion rRot;
                localPose[rootIndex].decompose(rTrans, rRot, rScale);
                
                Math::Quaternion finalRot = localRotDelta * rRot;
                rRot = rRot.slerp(finalRot, weight);
                localPose[rootIndex] = Math::Matrix4::fromTRS(rTrans, rRot, rScale);
            }
        }
    }
}

// Registration is handled in Humanoid.cpp to ensure the linker includes this TU.

} // namespace Engine
