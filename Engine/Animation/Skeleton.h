#pragma once

#include <vector>
#include <string>
#include "../Core/Math/Matrix4.h"

namespace Engine {
namespace Animation {

struct Bone {
    std::string name;
    int parentIndex = -1;
    Math::Matrix4 bindPoseLocalTransform;
    Math::Matrix4 inverseBindPoseWorldTransform;
};

class Skeleton {
public:
    std::vector<Bone> bones;

    int findBoneIndex(const std::string& name) const {
        for (int i = 0; i < (int)bones.size(); ++i) {
            if (bones[i].name == name) {
                return i;
            }
        }
        return -1;
    }

    // Compute world transforms from local transforms, respecting the hierarchy
    std::vector<Math::Matrix4> computeWorldTransforms(const std::vector<Math::Matrix4>& localTransforms) const;
};

} // namespace Animation
} // namespace Engine
