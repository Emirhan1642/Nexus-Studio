#include "AnimationTrack.h"
#include "Humanoid.h"
#include "../Reflection/ClassBuilder.h"

namespace Engine {

AnimationTrack::AnimationTrack() {
    name = "AnimationTrack";
}

std::shared_ptr<Humanoid> AnimationTrack::getHumanoidParent() const {
    auto parent = getParent();
    while (parent) {
        if (auto humanoid = std::dynamic_pointer_cast<Humanoid>(parent)) {
            return humanoid;
        }
        parent = parent->getParent();
    }
    return nullptr;
}

void AnimationTrack::onAddedToWorkspace() {
    Instance::onAddedToWorkspace();
}

void AnimationTrack::play(float blendTime) {
    if (auto humanoid = getHumanoidParent()) {
        if (clip) {
            humanoid->getAnimationPlayer().play(clip.get(), blendTime);
        }
    }
}

void AnimationTrack::stop(float fadeOutTime) {
    if (auto humanoid = getHumanoidParent()) {
        if (humanoid->getAnimationPlayer().getCurrentClip() == clip.get()) {
            humanoid->getAnimationPlayer().stop(fadeOutTime);
        }
    }
}

// Reflection registration
static void registerAnimationTrack() {
    Reflection::ClassBuilder<AnimationTrack>("AnimationTrack")
        .base("Instance")
        .method("Play", &AnimationTrack::play)
        .method("Stop", &AnimationTrack::stop);
}
static struct AnimationTrackRegister { AnimationTrackRegister() { registerAnimationTrack(); } } s_register;

} // namespace Engine
