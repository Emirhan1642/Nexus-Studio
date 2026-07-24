#pragma once

#include "Instance.h"
#include "../../Animation/AnimationClip.h"
#include "../../Animation/AnimationPlayer.h"
#include <memory>

namespace Engine {

class Humanoid;

class AnimationTrack : public Instance {
public:
    AnimationTrack();

    std::shared_ptr<Animation::AnimationClip> clip;
    
    int priority = 1000;
    float weight = 1.0f;
    std::vector<int> boneMask;
    
    // Play with crossfade blending duration
    void play(float blendTime = 0.2f);
    
    // Stop smoothly
    void stop(float fadeOutTime = 0.2f);

protected:
    void onAddedToWorkspace() override;

private:
    std::shared_ptr<Humanoid> getHumanoidParent() const;
};

} // namespace Engine
