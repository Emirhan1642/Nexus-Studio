#include "AnimationPlayer.h"
#include "../Core/Math/Quaternion.h"
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Animation {

void AnimationPlayer::play(AnimationClip* clip, void* sourceTrack, int priority, float blendDuration, float targetWeight, const std::vector<int>& boneMask, bool isAdditive) {
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
    track.isAdditive = isAdditive;
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
    Math::Vector3 pos1, scale1;
    Math::Quaternion rot1;
    fromPose.decompose(pos1, rot1, scale1);

    Math::Vector3 pos2, scale2;
    Math::Quaternion rot2;
    toPose.decompose(pos2, rot2, scale2);

    // Linear interpolate translation and scale
    Math::Vector3 finalPos = pos1 + (pos2 - pos1) * t;
    Math::Vector3 finalScale = scale1 + (scale2 - scale1) * t;
    
    // Spherical linear interpolate rotation
    Math::Quaternion finalRot = rot1.slerp(rot2, t);

    // Recompose
    return Math::Matrix4::fromTRS(finalPos, finalRot, finalScale);
}

Math::Matrix4 AnimationPlayer::blendAdditiveTransforms(const Math::Matrix4& basePose, const Math::Matrix4& additivePose, const Math::Matrix4& refPose, float weight) const {
    Math::Vector3 basePos, baseScale;
    Math::Quaternion baseRot;
    basePose.decompose(basePos, baseRot, baseScale);

    Math::Vector3 addPos, addScale;
    Math::Quaternion addRot;
    additivePose.decompose(addPos, addRot, addScale);

    Math::Vector3 refPos, refScale;
    Math::Quaternion refRot;
    refPose.decompose(refPos, refRot, refScale);

    // Delta = Additive - Ref
    Math::Vector3 deltaPos = addPos - refPos;
    Math::Quaternion deltaRot = refRot.inverse() * addRot;
    Math::Vector3 deltaScale = addScale / refScale;

    // Apply delta to base pose using weight
    Math::Vector3 finalPos = basePos + (deltaPos * weight);
    Math::Quaternion finalRot = baseRot * Math::Quaternion::identity().slerp(deltaRot, weight);
    
    // Lerp scale difference
    Math::Vector3 finalScale = baseScale * (Math::Vector3(1, 1, 1) + ((deltaScale - Math::Vector3(1, 1, 1)) * weight));

    return Math::Matrix4::fromTRS(finalPos, finalRot, finalScale);
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

                if (track.isAdditive) {
                    Math::Matrix4 refPose = skeleton.bones[i].bindPoseLocalTransform;
                    finalPoses[i] = blendAdditiveTransforms(finalPoses[i], trackPose, refPose, track.weight);
                } else {
                    // Blend with finalPoses[i] based on track weight
                    // Priority ensures we blend on top of lower priority animations
                    if (track.weight >= 1.0f) {
                        finalPoses[i] = trackPose;
                    } else {
                        finalPoses[i] = blendTransforms(finalPoses[i], trackPose, track.weight);
                    }
                }
            }
        }

        ++it;
    }

    return finalPoses;
}

} // namespace Animation
} // namespace Engine
