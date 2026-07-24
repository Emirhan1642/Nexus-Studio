#include "Skeleton.h"

namespace Engine {
namespace Animation {

std::vector<Math::Matrix4> Skeleton::computeWorldTransforms(const std::vector<Math::Matrix4>& localTransforms) const {
    std::vector<Math::Matrix4> worldTransforms(bones.size());
    for (size_t i = 0; i < bones.size(); ++i) {
        if (bones[i].parentIndex == -1) {
            worldTransforms[i] = localTransforms[i];
        } else {
            // Parent index must be less than current index for correct evaluation!
            worldTransforms[i] = worldTransforms[bones[i].parentIndex] * localTransforms[i];
        }
    }
    return worldTransforms;
}

} // namespace Animation
} // namespace Engine
