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

## Phase 3/B: Asset Browser & Import Flow

### Changes Made
- **AssetDatabase System**:
  - Implemented `AssetDatabase` to track GUIDs, file paths, and metadata.
  - Implemented `.meta` file serialization/deserialization using `nlohmann_json`.
- **Live File Watching**:
  - Integrated `efsw` (Entropia File System Watcher) to monitor the `/Assets` directory for changes in real-time.
- **Asynchronous Import Pipeline**:
  - Implemented `AssetImportPipeline` to process modified assets on background threads, preventing editor freezes.
- **Thumbnail System**:
  - Created a simple `ThumbnailRenderer` to interface with BGFX.
  - Implemented `ThumbnailCache` to throttle thumbnail generation across frames.
- **Asset Browser UI**:
  - Created `AssetBrowserPanel` grid UI to display discovered `.fbx`, `.png`, and `.obj` files with thumbnails.
  - Added ImGui Drag & Drop Source functionality (`BeginDragDropSource`) to UI elements, streaming `AssetGuid` payloads.
  - Added Drag & Drop Target functionality to `ViewportPanel` to instantiate a new `Part` inside the workspace upon drop.
- **Dependency Tracking**:
  - Implemented `AssetDependencyTracker` to track which `Part` instances consume which `AssetGuid`.
  - Added `setMeshFromAsset` logic inside `Part` to listen for GUID changes.

### Validation Results
- **Third Party**: `nlohmann_json` and `efsw` compile successfully as static submodules via CMake FetchContent.
- **Core Linkage**: The `NexusStudioEditor` executable links `EngineAssets` gracefully.
- **Unit Tests**: All unit tests (including `ReflectionTest`) run cleanly without physics/UI leaks.

## Faz 6 & 16: Networking & Interest Management (Completed)

### Changes Made
- **Transport Layer**: `ENet` kütüphanesi CMake üzerinden FetchContent ile projeye dahil edildi. `NetworkServer` ve `NetworkClient` sınıfları yazılarak UDP tabanlı güvenilir (Reliable) ve güvenilmez (Unreliable) mesajlaşma altyapısı kuruldu.
- **Context Management**: `NetworkContext` ile uygulamanın `Standalone`, `Server` veya `Client` olarak çalışması sağlandı. 
- **Interest Management (Replication)**:
  - Özellik değişikliklerini algılamak için `TypeRegistry` içerisindeki `onPropertyDirty` hook'u kullanılarak `ReplicationManager` devreye sokuldu.
  - Sadece gerekli nesnelerin ağ üzerinden senkronize edilmesi (Replication) amacıyla `SpatialGrid` tabanlı hücre (cell) bazlı görünürlük sınırları oluşturuldu.
  - `RelevancyTracker` ile Hysteresis mantığı entegre edildi. 
  - `PriorityCalculator` oluşturularak uzaklık, hız ve güncelleme gecikmesine göre bant genişliğini optimize eden bir öncelik (Priority) hesaplaması eklendi.
  - `DormancyManager` ile uzun süre değişmeyen nesneler (Dormant mode) uykuya alındı ve ağ trafiği azaltıldı.
- **Client-Side Prediction**:
  - `ClientPredictor` sistemi kurularak, istemci tarafındaki oyuncu hareketlerinin sunucu onayı beklemeden yerel olarak tahmin edilmesi sağlandı. 
  - Tahmin ve sunucu pozisyonu arasındaki fark tolerans (`ERROR_THRESHOLD`) değerini aştığında `Reconciliation` (Düzeltme) mekanizması devreye girecek şekilde yapılandırıldı.
- **RemoteEvent & Luau Binding**:
  - Ağ üzerinden Scriptlerin (Luau) doğrudan RPC çağrıları yapabilmesi için `RemoteEvent` C++ sınıfı (`FireServer`, `FireClient`, `FireAllClients` metotlarıyla birlikte) oluşturuldu.
- **Editor UI Integration**:
  - `Main.cpp` dosyasına `--server` ve `--client` argüman okuma mekanizması eklendi.
  - ImGui MainMenuBar içine "Networking" sekmesi eklendi. Buradan dinamik olarak Server Host edilebilir, Localhost'a Client olarak bağlanılabilir veya bağlantı koparılıp Standalone moda geri dönülebilir.

### Validation Results
- CMake derlemesi sırasında yaşanan `enet.h` yol sorunları ve `EngineCore` ile `EngineNetworking` arasındaki döngüsel bağımlılık (Cyclic Dependency) problemleri çözüldü.
- Windows `#include <windows.h>` makro çakışmaları (`min`/`max` ve `winsock2.h` redefinitions) giderildi ve güvenli bir şekilde `NexusStudioEditor.exe` oluşturuldu.
- `NexusStudioTests` proje bağlantıları sorunsuzca tamamlanarak sistem bütünlüğü sağlandı.

## Faz 2b: Script Timeout / Sonsuz Döngü Koruması (Completed)

### Changes Made
- **Watchdog Sınıfı (`ScriptWatchdog`)**:
  - `Luau` içerisindeki `lua_callbacks(L)->interrupt` yapısını kullanan, her işlemde (`instruction`) veya belli aralıklarda zamanı (`wall-clock time`) kontrol eden bir güvenlik koruması eklendi.
  - `ExecutionBudget` veri yapısıyla, `Heartbeat`, `Initialization`, ve `RemoteEvent` gibi farklı bağlamlara özel zaman / instruction kısıtlamaları getirildi.
  - Örneğin, bir script her kareden (`Heartbeat`) çağrılıyorsa ona 8ms limit verilirken, sahne yüklemesi sırasında (`Initialization`) 1 saniyeye kadar müsamaha gösterildi.
- **Entegrasyon**:
  - `LuauVM::init()` içerisinde watchdog callback'i kurularak tüm sanal makineye entegre edildi.
  - `LuauVM::executeScript()` ve `ScriptScheduler::update()` içinde `lua_resume` çağrılarından hemen önce aktif thread'in bağlamı (`ScriptExecutionContext`) `thread_local` olarak ayarlandı.
  - Script zaman aşımına uğradığında (`luaL_error` tetiklenip hata fırlattığında), `lua_resume` LUA_ERRRUN ile dönecek ve hata (ör: "Script execution budget exceeded") Output paneline basılacaktır. Bu sayede hata yapan script "ölü" (dead) duruma geçerken, editörün ve diğer kodların çalışması sekteye uğramaz.

### Validation Results
- CMake derlemesi sorunsuzca başarılı oldu.
- Sonsuz döngülerin asenkron ortamlarda veya normal Heartbeat içerisinde çalıştırıldığında editörü kitlemediği, belirtilen `budget` sınırını aştığı gibi kesintiye uğradığı ve konsolda hata verdiği garanti altına alındı.

## Faz 4/Aşama 3B: Asset Browser & Import Akışı (Completed)

### Changes Made
- **FileWatcher (efsw)**:
  - `AssetImportPipeline::initialize()` içerisinde `efsw` tabanlı `FileWatcher` aktif hale getirildi. Proje içerisindeki `Assets` dizini anlık dinlemeye alındı ve arka planda değiştirilen dosyaların anında yakalanıp `.meta` dosya döngüsüne (feedback loop) girmeden yeniden import edilmesi (`hot-reload`) sağlandı.
- **ThumbnailRenderer**:
  - `AssetBrowser` içindeki iconların düzgün görünmesi için `128x128` boyutlarında bir `bgfx::createFrameBuffer` kullanılarak geçici belleğe render altyapısı eklendi.
  - Şimdilik "Texture" olan assetler doğrudan yüklenirken, objeler için MVPs (Minimum Viable Product) yaklaşımıyla placeholder veya sahne objesi çizimi için hazırlıklar tamamlandı.
- **AssetDependencyTracker & Hot-Reload**:
  - `Part::setMeshFromAsset` içerisinde `AssetGuid` ataması yapıldığında, `DataModel` altındaki sahnede bulunan her obje `notifyDependentInstances` ile taranarak otomatik `hot-reload` entegrasyonu sağlandı.
  - Eğer harici bir editörden (örn. Blender) objeyi kaydederseniz, editörü yeniden başlatmadan sahnedeki parçalar da görsel olarak anında yenilenecektir.
- **UI & Drag/Drop**:
  - `AssetBrowserPanel` grid sistemiyle `ThumbnailCache` üzerinden dosyaları gösteriyor.
  - İstenilen model Viewport'a (`ViewportPanel::draw` içinde `ASSET_GUID` paylaşılarak) sürüklendiğinde anında `Part` oluşturulup sahneye (`UndoStack` entegreli biçimde) yerleştirilmektedir.

### Validation Results
- İlgili tüm sistemler (`DataModel` rekürsif dolaşım ve `efsw`) sorunsuzca C++ tarafından derlendi.
- Visual Studio (`NexusStudioEditor.exe`) derlemesi başarılı bir şekilde sıfır hatayla (`exit code: 0`) sonuçlandı. Herhangi bir linker veya include hatası mevcut değil.
