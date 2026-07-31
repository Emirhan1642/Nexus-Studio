# Tasks: FBX Import Assumptions

- `[x]` Update `AnimationClip.h` to use independent time arrays (positionTimes, rotationTimes, scaleTimes)
- `[x]` Update `AnimationClip.cpp` sampling logic to interpolate position, rotation, and scale independently
- `[x]` Update `SkeletalMeshImporter.cpp` to correctly parse independent keyframe tracks from Assimp
- `[x]` Update `AnimationTests.cpp` to initialize the updated `BoneKeyframes` structure
- `[x]` Run and verify `AnimationTest` suite
