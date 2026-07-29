# Inverse Kinematics (IK) Integration Plan

Inverse Kinematics (IK) allows character limbs (like arms and legs) to reach specific targets dynamically, which is crucial for procedural animation, foot placement, and aiming mechanics.

## User Review Required
> [!IMPORTANT]
> The IK functionality will be exposed through a new `IKControl` Instance type. This is consistent with Roblox's DataModel architecture. The `IKControl` instance will be added as a child to the `Humanoid` to apply IK to its skeleton.
> 
> **Does this component-based API approach meet your expectations?**

## Proposed Changes

### Engine/Core/DataModel
#### [NEW] [IKControl.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/IKControl.h)
- Create a new `Instance` subclass called `IKControl`.
- Properties:
  - `EndEffector`: `std::string` (The name of the bone to reach the target, e.g., "RightHand").
  - `TargetPosition`: `Math::Vector3` (The target position in object space).
  - `PoleVector`: `Math::Vector3` (The direction the elbow/knee should point in object space).
  - `Weight`: `float` (0.0 to 1.0, determines blending strength).
- Methods:
  - `void apply(Animation::Skeleton& skeleton, std::vector<Math::Matrix4>& localPose, const std::vector<Math::Matrix4>& worldPose)`: Executes the IK logic.

#### [NEW] [IKControl.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/IKControl.cpp)
- Implementation of the analytic 2-bone IK solver.
- It will extract the Root, Mid, and End bone positions from `worldPose`.
- Using vector math and the Law of Cosines, it calculates the required rotations to bend the Mid joint and aim the Root joint so that the End joint hits `TargetPosition`.
- Applies the delta rotations to `localPose`.
- Registers the class and its properties via `TypeRegistry`.

#### [MODIFY] [Humanoid.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.cpp)
- In the `Humanoid::update` method, after evaluating skeletal animations (`animationPlayer.evaluate`), it will compute the initial `worldPose`.
- Iterate through all children of `Humanoid`. For any child that is an `IKControl`, call its `apply(...)` method.
- Recompute the final `worldPose` if any IK controls were applied so that the rest of the body correctly follows the IK-adjusted joints.

#### [MODIFY] [CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/CMakeLists.txt)
- Add `IKControl.cpp` to the EngineCore build target.

## Verification Plan
### Automated Tests
- Run `cmake --build build --config Release` to verify the C++ syntax and successful compilation.
### Manual Verification
- The user can test it by creating an `IKControl` instance, setting its `EndEffector` to a valid bone name (e.g., "LeftHand"), and modifying `TargetPosition`. They should observe the limb dynamically reaching towards the target point in the engine/studio.
