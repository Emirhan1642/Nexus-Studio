#pragma once

#include <string>
#include <vector>
#include <string>
#include "../Core/Math/Vector3.h"
#include "../Core/Math/Quaternion.h"
#include "../Core/Math/Matrix4.h"

namespace Engine {
namespace Animation {

struct BoneKeyframes {
    int boneIndex;
    std::vector<float> times;
    std::vector<Math::Vector3> positions;
    std::vector<Math::Quaternion> rotations;
    std::vector<Math::Vector3> scales;
};

class AnimationClip {
public:
    std::string name;
    float duration = 0.0f;
    bool looping = true;
    bool isAdditive = false;
    std::vector<BoneKeyframes> boneTracks;

    const BoneKeyframes* findTrack(int boneIndex) const {
        for (const auto& track : boneTracks) {
            if (track.boneIndex == boneIndex) return &track;
        }
        return nullptr;
    }

    Math::Matrix4 sampleBone(int boneIndex, float time) const;
};

} // namespace Animation
} // namespace Engine
