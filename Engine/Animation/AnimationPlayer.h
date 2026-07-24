#pragma once

#include "AnimationClip.h"
#include "Skeleton.h"
#include <optional>
#include <vector>

namespace Engine {
namespace Animation {

struct PlayingTrack {
    AnimationClip* clip = nullptr;
    float time = 0.0f;
    float weight = 0.0f;
    float targetWeight = 1.0f;
    float weightSpeed = 5.0f;
    int priority = 1000; // Core (lowest)
    std::vector<int> boneMask; // If empty, applies to all bones
    
    // Identifier for tracking which AnimationTrack this comes from (optional but useful)
    void* sourceTrack = nullptr; 
};

class AnimationPlayer {
public:
    // Play an animation with specific parameters
    void play(AnimationClip* clip, void* sourceTrack, int priority = 1000, float blendDuration = 0.2f, float targetWeight = 1.0f, const std::vector<int>& boneMask = {});
    
    // Stop an animation from a specific source track
    void stop(void* sourceTrack, float fadeOutTime = 0.2f);
    
    // Evaluates current frame pose (returns local transforms for each bone)
    std::vector<Math::Matrix4> evaluate(const Skeleton& skeleton, float deltaTime);

private:
    Math::Matrix4 blendTransforms(const Math::Matrix4& fromPose, const Math::Matrix4& toPose, float t) const;

    std::vector<PlayingTrack> activeTracks;
};

} // namespace Animation
} // namespace Engine
