#include "AnimationPlayer.h"
#include "../Core/Math/Quaternion.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Animation {

void AnimationPlayer::play(AnimationClip* clip, void* sourceTrack, int priority, float blendDuration, float targetWeight, const std::vector<int>& boneMask) {
    if (!clip) return;

    // Remove if already exists from this source
    stop(sourceTrack, 0.0f);

    PlayingTrack track;
    track.clip = clip;
    track.time = 0.0f;
    track.weight = blendDuration > 0.0f ? 0.0f : targetWeight;
    track.targetWeight = targetWeight;
    track.weightSpeed = blendDuration > 0.0f ? (1.0f / blendDuration) : 1000.0f;
    track.priority = priority;
    track.boneMask = boneMask;
    track.sourceTrack = sourceTrack;

    activeTracks.push_back(track);
}

void AnimationPlayer::stop(void* sourceTrack, float fadeOutTime) {
    for (auto& track : activeTracks) {
        if (track.sourceTrack == sourceTrack) {
            track.targetWeight = 0.0f;
            track.weightSpeed = fadeOutTime > 0.0f ? (1.0f / fadeOutTime) : 1000.0f;
        }
    }
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
    std::vector<Math::Matrix4> finalPoses;
    finalPoses.resize(skeleton.bones.size());

    // Initialize with bind poses
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        finalPoses[i] = skeleton.bones[i].bindPoseLocalTransform;
    }

    if (activeTracks.empty()) {
        return finalPoses;
    }

    // Sort tracks by priority (lowest to highest)
    std::sort(activeTracks.begin(), activeTracks.end(), [](const PlayingTrack& a, const PlayingTrack& b) {
        return a.priority < b.priority;
    });

    for (auto it = activeTracks.begin(); it != activeTracks.end(); ) {
        auto& track = *it;

        // Update time
        track.time += deltaTime;
        if (track.time >= track.clip->duration) {
            if (track.clip->looping) {
                track.time = std::fmod(track.time, track.clip->duration);
            } else {
                track.time = track.clip->duration; // Keep at last frame
            }
        }

        // Update weight (Fade in / Fade out)
        if (track.weight < track.targetWeight) {
            track.weight += track.weightSpeed * deltaTime;
            if (track.weight > track.targetWeight) track.weight = track.targetWeight;
        } else if (track.weight > track.targetWeight) {
            track.weight -= track.weightSpeed * deltaTime;
            if (track.weight < track.targetWeight) track.weight = track.targetWeight;
        }

        // Remove if fully faded out
        if (track.targetWeight <= 0.0f && track.weight <= 0.0f) {
            it = activeTracks.erase(it);
            continue;
        }

        // Evaluate and blend track poses
        if (track.weight > 0.0f) {
            for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                // Check if bone is masked
                if (!track.boneMask.empty()) {
                    if (std::find(track.boneMask.begin(), track.boneMask.end(), static_cast<int>(i)) == track.boneMask.end()) {
                        continue; // Bone is masked out, skip it
                    }
                }

                Math::Matrix4 trackPose = track.clip->sampleBone(static_cast<int>(i), track.time);

                // Blend with finalPoses[i] based on track weight
                // Priority ensures we blend on top of lower priority animations
                if (track.weight >= 1.0f) {
                    finalPoses[i] = trackPose;
                } else {
                    finalPoses[i] = blendTransforms(finalPoses[i], trackPose, track.weight);
                }
            }
        }

        ++it;
    }

    return finalPoses;
}

} // namespace Animation
} // namespace Engine
