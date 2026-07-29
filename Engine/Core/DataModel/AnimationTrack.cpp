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

void AnimationTrack::addBoneMask(const std::string& boneName) {
    if (std::find(maskedBones.begin(), maskedBones.end(), boneName) == maskedBones.end()) {
        maskedBones.push_back(boneName);
    }
}

void AnimationTrack::removeBoneMask(const std::string& boneName) {
    maskedBones.erase(
        std::remove(maskedBones.begin(), maskedBones.end(), boneName), 
        maskedBones.end()
    );
}

void AnimationTrack::play(float blendTime) {
    if (auto humanoid = getHumanoidParent()) {
        if (clip) {
            std::vector<int> resolvedMask;
            const auto& skeleton = humanoid->getSkeleton();
            
            auto addBoneAndChildren = [&](auto& self, int boneIndex) -> void {
                if (std::find(resolvedMask.begin(), resolvedMask.end(), boneIndex) == resolvedMask.end()) {
                    resolvedMask.push_back(boneIndex);
                }
                if (maskRecursive) {
                    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                        if (skeleton.bones[i].parentIndex == boneIndex) {
                            self(self, static_cast<int>(i));
                        }
                    }
                }
            };

            for (const auto& name : maskedBones) {
                for (size_t i = 0; i < skeleton.bones.size(); ++i) {
                    if (skeleton.bones[i].name == name) {
                        addBoneAndChildren(addBoneAndChildren, static_cast<int>(i));
                    }
                }
            }
            
            bool applyAdditive = isAdditive || clip->isAdditive;
            humanoid->getAnimationPlayer().play(clip.get(), this, priority, blendTime, weight, resolvedMask, applyAdditive);
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
        .property("MaskRecursive", &AnimationTrack::maskRecursive)
        .property("IsAdditive", &AnimationTrack::isAdditive)
        .method("AddBoneMask", &AnimationTrack::addBoneMask)
        .method("RemoveBoneMask", &AnimationTrack::removeBoneMask)
        .method("Play", &AnimationTrack::play)
        .method("Stop", &AnimationTrack::stop);
}
static struct AnimationTrackRegister { AnimationTrackRegister() { registerAnimationTrack(); } } s_register;

} // namespace Engine
