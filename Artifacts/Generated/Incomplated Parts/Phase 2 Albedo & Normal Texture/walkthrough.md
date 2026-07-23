# Nexus Studio - Walkthrough

## Phase 3 & 5 Completion: Signals, Lua Connect, and Physics Touched Events

### Changes Made
- **Reflection System Upgrade**:
  - Added `SignalDescriptor` to `TypeRegistry.h` and `signal()` method to `ClassBuilder.h` to allow registering event signals inside the Reflection system.
  - Registered `Touched` signal inside `Part.cpp`.
- **Luau Signal & Event Binding (`:Connect()`)**:
  - Implemented `kSignalTag` and `SignalUserdata` in `InstanceBinding.cpp`.
  - Hooked up `__index` to correctly return a Signal Object when accessing registered signals (like `Part.Touched`).
  - Implemented a `Connect` C-function for Signals, which takes a Luau function reference and safely binds it to the C++ `Engine::Signal` event loop.
- **Physics Contact Processing (`Touched`)**:
  - Updated `PhysicsWorld::step()` to call `PendingContactEvents::instance().drainAll()` after stepping the Jolt simulation.
  - Mapped the returned `ContactEvent` Jolt Body IDs to `Part` instances in the `DataModel`.
  - Connected the collisions to the DataModel, firing the `Touched` signal recursively for each pair of colliding parts (`part1->Touched.fire({ part2 })`).

## Phase 5: Physics Engine (Jolt) & Play/Stop Mechanics

### Changes Made
- **Jolt Physics Initialization**: 
  - Separated Jolt's initialization (`initJolt`) logic to run *before* the `DataModel` elements are spawned. This fixed a critical crash occurring due to `Part` instances trying to create physics bodies before the physics world existed.
  - Linked `<iostream>` properly inside `PhysicsWorld.cpp` to resolve `std::cout` linker errors.
- **Play/Stop System**:
  - Implemented a snapshot mechanism in `Main.cpp` to pause the physics engine during Edit Mode.
  - Added a `Simulation` menu to the top `ImGui::MainMenuBar` with a **Play/Stop** button (shortcut: `F5`).
  - When starting Play mode, all `Part` positions are saved. When stopping, positions are restored and the physics bodies are reset (velocities nullified) via the new `Part::resetPhysics()` method.
- **Anchored Property Integration**:
  - Exposed `getAnchored` and `setAnchored` methods to the `Part` class.
  - Hooked the setter into the Reflection system (`propertyAccessor`). 
  - Changing the `Anchored` state now immediately updates the corresponding `JPH::Body` motion type (Static vs Dynamic) and physics layer (NON_MOVING vs MOVING).
  - Set the `Ground` part to be Anchored by default in the test scene.
- **Visual Polish & Rendering Fixes**:
  - Fixed an issue where the `Explorer` and `Properties` panels overlapped with the Main Menu Bar by changing their initialization to use `ImGui::GetMainViewport()->WorkPos` and `WorkSize`.
  - **Fixed PBR Shader Artifacts**: The test cube meshes were displaying a disco-like green/brown visual artifact due to sharing only 8 vertices and having incorrect normals. Rewrote the mesh array in `Renderer.cpp` to use 24 independent vertices with proper front-facing normals for all 6 faces, and pure white default vertex colors to let the shader's material colors work correctly.
  - Fixed a culling bug where the front faces were invisible due to mismatched winding orders (Clockwise vs Counter-Clockwise). Reversed the `s_cubeIndices` array so the outside of the cubes render correctly instead of the inside.

### Validation Results
- The project successfully builds (`NexusStudioEditor.exe`).
- Pressing `F5` starts the physics simulation correctly (cubes fall down and hit the ground).
- Pressing `F5` again stops the simulation and resets everything to its initial state seamlessly.
- Cubes with `Anchored` = True stay in the air completely static, and those with `Anchored` = False fall down.
- Cubes look perfectly opaque, solid, and uniformly lit without graphical glitches.
- `Part.Touched` signals are accurately fired with `otherPart` references.

### Next Steps
1. Run `./build/bin/Debug/NexusStudioEditor.exe`.
2. Observe the `MyCube1` and `Ground` instances in the Explorer.
3. Click `Play (F5)` in the Simulation menu to watch physics in action.
4. Experiment with changing `Anchored` property of `MyCube1` via the Properties Panel during edit mode.

## Phase 5: Constraints (Weld, Hinge, Spring)

### Changes Made
- **Base Constraint System (`Constraint`)**:
  - Created an abstract `Constraint` base class deriving from `Instance`.
  - Added properties: `Visible`, `Enabled`, `Part0`, `Part1`.
  - Used `std::weak_ptr<Instance>` for object references to prevent memory leaks when parts are destroyed.
  - Implemented `inWorkspace` lifecycle tracking to automatically build or destroy physics constraints when added/removed from the DataModel.
  - Extended `ClassBuilder` with `objectPropertyAccessor` to allow calling setters/getters on object reference changes.
- **WeldConstraint**:
  - Implemented `JPH::FixedConstraintSettings` to lock two parts together dynamically.
- **HingeConstraint**:
  - Implemented `JPH::HingeConstraintSettings` to allow rotation along a specific axis (Y-axis MVP) with a shared anchor point.
- **SpringConstraint**:
  - Implemented `JPH::DistanceConstraintSettings` using spring mechanics (`FrequencyAndDamping`). Exposed `FreeLength`, `Stiffness`, and `Damping` properties.
- **Viewport Visualization**:
  - Added 3D-to-2D projection in `ViewportPanel.cpp`.
  - Implemented recursive DataModel traversal to find all active constraints.
  - Draws a green line connecting `Part0` and `Part1` of any `Constraint` if `Visible` is true.
  - Added a "Show Constraints" toggle checkbox to the top-left corner of the Viewport to toggle the visual guides on or off globally.

### Validation Results
- Code compiles successfully without abstract-class instantiation errors or `DataModel` scope errors.
- Visual lines correctly project and follow 3D parts as the camera moves.

## Phase 2: Albedo & Normal Texture Mapping (PBR)

### Changes Made
- **DataModel & Properties**:
  - Removed the simple enum `Material`.
  - Added full PBR property properties to `Part`: `AlbedoColor`, `Metallic`, `Roughness`, `EmissiveStrength`.
  - Added texture path properties: `AlbedoTexture`, `NormalTexture`, `MetallicTexture`, `RoughnessTexture`.
  - Registered all these new properties in the `ClassBuilder` to show up as `InputText` and sliders/drag fields in the Properties Panel.
- **Renderer Updates**:
  - Created `MaterialData` struct to hold BGFX `TextureHandle` objects.
  - Implemented `RendererSystem::getTexture()` which loads and caches textures using BGFX `bimg` via `bgfx_utils.h` (`loadTexture`).
  - Added `example-common` dependency to `EngineRenderer`'s `CMakeLists.txt` to support the utility headers.
  - Expanded `PosColorTexCoordVertex` to support `u, v` coordinates and updated `s_cubeVertices` accordingly.
- **Shader Pipeline**:
  - Created 4 new Sampler Uniforms (`s_texColor`, `s_texNormal`, `s_texMetallic`, `s_texRoughness`).
  - Created a `u_textureFlags` vec4 uniform to branch between using solid colors/scalars vs. reading from textures dynamically per mesh.
  - Updated `varying.def.sc` to correctly transport `a_texcoord0` to `v_texcoord0`.
  - Recompiled `vs_pbr.sc` and `fs_pbr.sc` to apply textures at runtime and fallback to `u_albedoRoughness` and `u_metallicEmissive` if no texture is assigned.

### Validation Results
- Shaders compile successfully with `shaderc`.
- Project builds cleanly with no linker errors for `bimg` or `example-common`.
- Textures can now be provided via standard system paths inside the Editor's Properties panel.
