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
            humanoid->getAnimationPlayer().play(clip.get(), this, priority, blendTime, weight, boneMask);
        }
    }
}

void AnimationTrack::stop(float fadeOutTime) {
    if (auto humanoid = getHumanoidParent()) {
        humanoid->getAnimationPlayer().stop(this, fadeOutTime);
    }
}

// Reflection registration
static void registerAnimationTrack() {
    Reflection::ClassBuilder<AnimationTrack>("AnimationTrack")
        .base("Instance")
        .property("Priority", &AnimationTrack::priority)
        .property("Weight", &AnimationTrack::weight)
        // Array property for boneMask could be supported via a different system if not primitives, 
        // but skipping it in generic reflection if arrays aren't fully tested, or registering it if supported:
        // .arrayProperty("BoneMask", &AnimationTrack::boneMask)
        .method("Play", &AnimationTrack::play)
        .method("Stop", &AnimationTrack::stop);
}
static struct AnimationTrackRegister { AnimationTrackRegister() { registerAnimationTrack(); } } s_register;

} // namespace Engine
