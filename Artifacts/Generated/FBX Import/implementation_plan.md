# [Goal Description]

Decouple Position, Rotation, and Scale keyframes within `BoneKeyframes` to handle FBX files correctly where these properties are optimized and do not share the exact same keyframe counts or timestamps.

## Proposed Changes

Currently, `BoneKeyframes` uses a single `std::vector<float> times` array for positions, rotations, and scales. We will separate this into three independent time arrays so that an animation track can have a different number of rotation keys vs position keys.

### Core Animation Structure Updates

#### [MODIFY] [AnimationClip.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationClip.h)
- Replace `std::vector<float> times;` with three independent arrays:
  - `std::vector<float> positionTimes;`
  - `std::vector<float> rotationTimes;`
  - `std::vector<float> scaleTimes;`

#### [MODIFY] [AnimationClip.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationClip.cpp)
- Update `findSurroundingKeyframes` to be a templated/standalone utility.
- Update `AnimationClip::sampleBone()` to sample positions using `positionTimes`, rotations using `rotationTimes`, and scales using `scaleTimes`.
- If an array is empty, it should safely fall back to the bone's bind pose or an identity value (e.g. `Vector3(0,0,0)` for pos, `identity` for rot, `Vector3(1,1,1)` for scale).

### Importer and Tests Updates

#### [MODIFY] [SkeletalMeshImporter.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/Importers/SkeletalMeshImporter.cpp)
- Modify `importFBX`'s animation processing loop.
- Instead of using a single loop for `channel->mNumPositionKeys` and assuming rotations/scales match it, use three independent loops to populate positions, rotations, and scales respectively from Assimp's `mPositionKeys`, `mRotationKeys`, and `mScalingKeys`.

#### [MODIFY] [AnimationTests.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Tests/AnimationTests.cpp)
- Update test cases (`TestAnimationPlayer`, `TestAnimationBlending`, `TestBoneMasking`) to initialize `.positionTimes`, `.rotationTimes`, and `.scaleTimes` instead of the single `.times` array.

## Verification Plan

### Automated Tests
- Run `cmake --build build --config Release --target NexusStudioTests`
- Execute `NexusStudioTests.exe --gtest_filter=AnimationTest.*` to ensure the sampling logic and blending works correctly with the newly split timeline structure.
