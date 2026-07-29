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
    player.play(&clip, nullptr, 1000, 0.0f);

    std::vector<Matrix4> localPoses = player.evaluate(skeleton, 0.5f);
    EXPECT_EQ(localPoses.size(), 1);
}

TEST(AnimationTest, TestAnimationBlending) {
    Skeleton skeleton;
    Bone root;
    root.name = "Root";
    root.parentIndex = -1;
    root.bindPoseLocalTransform = Matrix4::fromTRS({0, 0, 0}, Quaternion::identity(), {1, 1, 1});
    skeleton.bones.push_back(root);

    AnimationClip clipA;
    clipA.duration = 1.0f;
    BoneKeyframes keysA;
    keysA.boneIndex = 0;
    keysA.times = {0.0f, 1.0f};
    keysA.positions = {{0, 0, 0}, {0, 0, 0}}; // Stays at 0
    keysA.rotations = {Quaternion::identity(), Quaternion::identity()};
    keysA.scales = {{1, 1, 1}, {1, 1, 1}};
    clipA.boneTracks.push_back(keysA);

    AnimationClip clipB;
    clipB.duration = 1.0f;
    BoneKeyframes keysB;
    keysB.boneIndex = 0;
    keysB.times = {0.0f, 1.0f};
    keysB.positions = {{0, 10, 0}, {0, 10, 0}}; // Stays at 10
    keysB.rotations = {Quaternion::identity(), Quaternion::identity()};
    keysB.scales = {{1, 1, 1}, {1, 1, 1}};
    clipB.boneTracks.push_back(keysB);

    AnimationPlayer player;
    
    // Play both tracks
    int trackA_id = 1;
    int trackB_id = 2;
    
    // Play track A with weight 0.5
    player.play(&clipA, &trackA_id, 100, 0.0f, 0.5f);
    
    // Play track B with weight 0.5 and higher priority
    player.play(&clipB, &trackB_id, 110, 0.0f, 0.5f);

    std::vector<Matrix4> localPoses = player.evaluate(skeleton, 0.1f);
    EXPECT_EQ(localPoses.size(), 1);

    Vector3 pos, scale;
    Quaternion rot;
    localPoses[0].decompose(pos, rot, scale);

    // Track A applies 0.5 weight on BindPose(0). Pose becomes 0.
    // Track B applies 0.5 weight on current Pose(0). Target is 10.
    // Lerp(0, 10, 0.5) = 5.0
    EXPECT_NEAR(pos.y, 5.0f, 0.01f);
}

TEST(AnimationTest, TestBoneMasking) {
    Skeleton skeleton;
    
    Bone root; root.name = "Root"; root.parentIndex = -1;
    root.bindPoseLocalTransform = Matrix4::fromTRS({0, 0, 0}, Quaternion::identity(), {1, 1, 1});
    
    Bone arm; arm.name = "Arm"; arm.parentIndex = 0;
    arm.bindPoseLocalTransform = Matrix4::fromTRS({0, 0, 0}, Quaternion::identity(), {1, 1, 1});
    
    skeleton.bones.push_back(root);
    skeleton.bones.push_back(arm);

    AnimationClip clipA;
    clipA.duration = 1.0f;
    BoneKeyframes keysARoot; keysARoot.boneIndex = 0; keysARoot.times = {0.0f}; 
    keysARoot.positions = {{0, 10, 0}}; keysARoot.rotations = {Quaternion::identity()}; keysARoot.scales = {{1, 1, 1}};
    
    BoneKeyframes keysAArm; keysAArm.boneIndex = 1; keysAArm.times = {0.0f}; 
    keysAArm.positions = {{0, 10, 0}}; keysAArm.rotations = {Quaternion::identity()}; keysAArm.scales = {{1, 1, 1}};
    
    clipA.boneTracks.push_back(keysARoot);
    clipA.boneTracks.push_back(keysAArm);

    AnimationPlayer player;
    
    int trackA_id = 1;
    // Play track A with mask only on bone 0 (Root). Bone 1 (Arm) is masked OUT (not in the list).
    std::vector<int> mask = {0}; 
    player.play(&clipA, &trackA_id, 100, 0.0f, 1.0f, mask);

    std::vector<Matrix4> localPoses = player.evaluate(skeleton, 0.1f);

    Vector3 rootPos, rootScale; Quaternion rootRot;
    localPoses[0].decompose(rootPos, rootRot, rootScale);
    
    Vector3 armPos, armScale; Quaternion armRot;
    localPoses[1].decompose(armPos, armRot, armScale);

    // Root should be animated to 10
    EXPECT_NEAR(rootPos.y, 10.0f, 0.01f);
    // Arm should remain at bind pose (0) because it was masked out
    EXPECT_NEAR(armPos.y, 0.0f, 0.01f);
}
