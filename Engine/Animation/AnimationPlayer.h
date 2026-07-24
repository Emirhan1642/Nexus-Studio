#pragma once

#include "AnimationClip.h"
#include "Skeleton.h"
#include <optional>
#include <vector>

namespace Engine {
namespace Animation {

struct AnimationTransition {
    float blendDuration;
    float elapsedTime = 0.0f;
    AnimationClip* fromClip;
    AnimationClip* toClip;
    float fromClipTime;
    float toClipTime;

    bool isComplete() const { return elapsedTime >= blendDuration; }
};

class AnimationPlayer {
public:
    void play(AnimationClip* clip, float blendDuration = 0.2f);
    void stop(float fadeOutTime = 0.2f); // Stops current animation smoothly if implemented (simplified for MVP)

    // Evaluates current frame pose (returns local transforms for each bone)
    std::vector<Math::Matrix4> evaluate(const Skeleton& skeleton, float deltaTime);

    AnimationClip* getCurrentClip() const { return currentClip; }
    
private:
    Math::Matrix4 blendTransforms(const Math::Matrix4& fromPose, const Math::Matrix4& toPose, float t) const;

    AnimationClip* currentClip = nullptr;
    float currentTime = 0.0f;
    std::optional<AnimationTransition> activeTransition;
};

} // namespace Animation
} // namespace Engine
