#include "AnimationPlayer.h"
#include "../Core/Math/Quaternion.h"
#include <iostream>
#include <cmath>

namespace Engine {
namespace Animation {

void AnimationPlayer::play(AnimationClip* clip, float blendDuration) {
    if (currentClip == clip) return;

    if (currentClip && blendDuration > 0.0f) {
        activeTransition = AnimationTransition{blendDuration, 0.0f, currentClip, clip, currentTime, 0.0f};
    } else {
        activeTransition.reset();
    }
    
    currentClip = clip;
    currentTime = 0.0f;
}

void AnimationPlayer::stop(float fadeOutTime) {
    // For MVP, just stop immediately
    currentClip = nullptr;
    activeTransition.reset();
    currentTime = 0.0f;
}

Math::Matrix4 AnimationPlayer::blendTransforms(const Math::Matrix4& fromPose, const Math::Matrix4& toPose, float t) const {
    // A simplified blend extracting pos/scale and interpolating. 
    // In a real engine, we'd decompose Matrix4 into TRS, lerp/slerp, and recompose.
    // Assuming simple affine matrices without shear:
    Math::Vector3 p1 = fromPose.getTranslation();
    Math::Vector3 p2 = toPose.getTranslation();
    Math::Vector3 pos = p1 + (p2 - p1) * t;

    // To do a proper rot/scale blend, we need matrix decomposition.
    // For this MVP, we will only blend translation, which works okay for simple rigid transitions
    // However, for skeletal animation we really need rotation blending.
    // To do this properly without full decompose, we can use quaternion slerp on rotations.
    // For now we will approximate by returning toPose if t > 0.5, but let's implement a hacky decompose.
    
    // Extrapolate rotation assuming scale is 1.0.
    Math::Matrix4 result = toPose;
    result.m[12] = pos.x;
    result.m[13] = pos.y;
    result.m[14] = pos.z;
    
    return result; 
    // TODO: implement proper decompose and slerp for blendTransforms if needed. 
    // Usually AnimationPlayer works on decomposed Bone keys (TRS) BEFORE composing Matrix4 to blend properly.
}

std::vector<Math::Matrix4> AnimationPlayer::evaluate(const Skeleton& skeleton, float deltaTime) {
    std::vector<Math::Matrix4> finalPose(skeleton.bones.size());

    if (!currentClip) {
        for (size_t i = 0; i < skeleton.bones.size(); ++i) {
            finalPose[i] = skeleton.bones[i].bindPoseLocalTransform;
        }
        return finalPose;
    }

    currentTime += deltaTime;
    if (currentClip->looping && currentClip->duration > 0.0f) {
        currentTime = std::fmod(currentTime, currentClip->duration);
    }

    if (activeTransition && !activeTransition->isComplete()) {
        activeTransition->elapsedTime += deltaTime;
        float t = activeTransition->elapsedTime / activeTransition->blendDuration;

        for (int i = 0; i < (int)skeleton.bones.size(); i++) {
            Math::Matrix4 fromPose = activeTransition->fromClip->sampleBone(i, activeTransition->fromClipTime);
            Math::Matrix4 toPose = activeTransition->toClip->sampleBone(i, currentTime);
            finalPose[i] = blendTransforms(fromPose, toPose, t);
        }
        
        if (activeTransition->isComplete()) {
            activeTransition.reset();
        }
    } else {
        for (int i = 0; i < (int)skeleton.bones.size(); i++) {
            finalPose[i] = currentClip->sampleBone(i, currentTime);
        }
    }
    
    return finalPose;
}

} // namespace Animation
} // namespace Engine
