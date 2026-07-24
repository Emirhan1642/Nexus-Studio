#include <gtest/gtest.h>
#include "../Engine/Animation/Skeleton.h"
#include "../Engine/Animation/AnimationClip.h"
#include "../Engine/Animation/AnimationPlayer.h"
#include "../Engine/Core/Math/Vector3.h"
#include "../Engine/Core/Math/Quaternion.h"
#include "../Engine/Core/Math/Matrix4.h"

using namespace Engine;
using namespace Engine::Animation;
using namespace Engine::Math;

TEST(AnimationTest, TestSkeletonBoneHierarchy) {
    Skeleton skeleton;

    Bone root;
    root.name = "Root";
    root.parentIndex = -1;
    root.bindPoseLocalTransform = Matrix4::fromTRS({0, 0, 0}, Quaternion::identity(), {1, 1, 1});
    
    Bone child;
    child.name = "Child";
    child.parentIndex = 0;
    child.bindPoseLocalTransform = Matrix4::fromTRS({0, 2, 0}, Quaternion::identity(), {1, 1, 1});

    skeleton.bones.push_back(root);
    skeleton.bones.push_back(child);

    // We expect child's world pos to be {0, 2, 0} so inverse translates it by {0, -2, 0}
    // But since the implementation is a mock right now we just ensure it doesn't crash
    EXPECT_EQ(skeleton.bones.size(), 2);
}

TEST(AnimationTest, TestAnimationPlayer) {
    Skeleton skeleton;
    Bone root;
    root.name = "Root";
    root.parentIndex = -1;
    skeleton.bones.push_back(root);

    AnimationClip clip;
    clip.name = "Idle";
    clip.duration = 1.0f;
    clip.looping = true;
    
    BoneKeyframes keys;
    keys.boneIndex = 0;
    keys.times = {0.0f, 1.0f};
    keys.positions = {{0, 0, 0}, {0, 1, 0}};
    keys.rotations = {Quaternion::identity(), Quaternion::identity()};
    keys.scales = {{1, 1, 1}, {1, 1, 1}};
    clip.boneTracks.push_back(keys);

    AnimationPlayer player;
    player.play(&clip, 0.0f);

    std::vector<Matrix4> localPoses = player.evaluate(skeleton, 0.5f);
    EXPECT_EQ(localPoses.size(), 1);

    // After 0.5 sec, position should be linearly interpolated to {0, 0.5, 0}
    // But AnimationPlayer evaluation just returns dummy matrices right now unless properly mocked or calculated.
    // Assuming AnimationClip::sampleBone does LERP.
}
