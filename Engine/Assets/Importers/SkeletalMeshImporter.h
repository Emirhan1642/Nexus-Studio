#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../Animation/Skeleton.h"
#include "../../Animation/AnimationClip.h"

namespace Engine {
namespace Assets {

struct SkinnedVertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    uint8_t boneIndices[4];
    float boneWeights[4];
};

struct ImportedSkeletalMesh {
    Animation::Skeleton skeleton;
    std::vector<Animation::AnimationClip> clips;
    std::vector<SkinnedVertex> vertices;
    std::vector<std::vector<uint32_t>> lodIndices; // lodIndices[0] is original, [1] is first LOD, etc.
};

class SkeletalMeshImporter {
public:
    static ImportedSkeletalMesh importFBX(const std::string& path);
};

} // namespace Assets
} // namespace Engine
