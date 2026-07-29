# Animation Blending & Bone-Based Masking System

Animasyon geçişleri (blending) ve kemik bazlı maskeleme (bone masking) sisteminin motor düzeyinde tasarlanması ve Humanoid ile entegre edilmesi için bir plan oluşturdum. Matematiksel altyapı (`AnimationPlayer`) büyük ölçüde hazır olsa da, oyun geliştiricilerinin bunu kolayca kullanabileceği bir mimariye (`Animator` ve `AnimationTrack` entegrasyonuna) dönüştürmemiz gerekiyor.

## User Review Required
> [!IMPORTANT]
> `AnimationTrack` objelerini DataModel hiyerarşisinde `Humanoid`'in bir alt objesi olarak tutmaya devam edeceğiz. Böylece Reflection üzerinden Inspector'da ayarları değiştirilebilir olacak. Kemik maskeleme için (örneğin sadece "Sağ Kol" animasyonu oynatmak için) String (isim) tabanlı bir maskeleme API'si önermekteyim. Lütfen bu tasarım kararını onaylayın.

## Proposed Changes

### 1. `AnimationTrack` Geliştirmeleri (Bone Mask API)
Geliştiricilerin index (sayı) yerine kemik isimleriyle maskeleme yapabilmesi için yeni fonksiyonlar eklenecek.
#### [MODIFY] [AnimationTrack.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/AnimationTrack.h) ve `.cpp`
- `addBoneToMask(const std::string& boneName, bool recursive = true)` metodu eklenecek.
- `removeBoneFromMask(const std::string& boneName, bool recursive = true)` metodu eklenecek.
- Bu metodlar, ebeveyn hiyerarşisindeki `Skeleton`'dan (Humanoid üzerinden) kemik isimlerini indexlere çevirip `AnimationPlayer`'ın maskeleme listesine kaydedecek.
- Reflection API'ye `AddBoneMask` metodu bağlanacak.

### 2. `Animator` veya `Humanoid` State Entegrasyonu
Karakterin yürüme, durma, zıplama gibi durumları (state) değiştiğinde, animasyonların yumuşak geçişle (cross-fade blending) birbirine karışması sağlanacak.
#### [MODIFY] [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.h) ve `.cpp`
- Default animasyonları tutmak için referanslar (`std::shared_ptr<AnimationClip> idleAnimation`, `walkAnimation` vb.) eklenecek.
- `Humanoid`'in kendi `AnimationTrack`'leri (Örn: `idleTrack`, `walkTrack`) yaratılıp saklanacak.
- `HumanoidStateMachine` durumu değiştiğinde (örneğin `Idle` -> `Walking`), eski animasyon `stop(0.2f)` ile durdurulup yeni animasyon `play(0.2f)` ile başlatılacak. `AnimationPlayer`, aradaki 0.2 saniyede Quaternion Slerp ve Vector Lerp kullanarak pürüzsüz geçiş (blending) yapacak.

### 3. Animasyon Çözücü Geliştirmeleri (Math & Logic)
#### [MODIFY] [AnimationPlayer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationPlayer.cpp)
- `evaluate()` içerisinde, kemik maskelemesi yapılırken masked-out (devre dışı bırakılmış) kemiklerin alt (child) kemiklerine de otomatik olarak etkinin aktarılmasını (veya isteğe bağlı kesilmesini) sağlayacak hiyerarşik destek kontrolü yapılacak.
- Aynı önceliğe (priority) sahip birden fazla animasyon aynı anda çalıyorsa, ağırlıklarına (weight) göre normalize edilip (Örn: %50 Walk, %50 Aim) harmanlanması sağlanacak.

# Asset Browser Integration for Skeletal Meshes and Animations

This plan details the implementation for allowing users to view, select, and use internal sub-assets (like Animations and Meshes) from an imported `.fbx` file directly through the Editor's Asset Browser.

## Goal
To treat `.fbx` files as "folders" in the Asset Browser, allowing developers to double-click an FBX and see its internal components (`SkeletalMesh`, `Skeleton`, `AnimationClip`s). These sub-assets will have their own Virtual GUIDs, allowing them to be dragged and dropped into property fields (like `idleAnimation` in `Humanoid`).

## User Review Required
> [!IMPORTANT]
> The AssetBrowser will change slightly. Double-clicking an FBX file will now navigate "inside" the file to show its animations and meshes, rather than doing nothing. A "Back" button will be added to navigate out.

## Proposed Changes

---

### Asset Database (Virtual Sub-Assets)

#### [MODIFY] [AssetDatabase.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetDatabase.h)
#### [MODIFY] [AssetDatabase.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetDatabase.cpp)
- Add the concept of "Virtual Assets" (sub-assets). Virtual assets won't generate their own `.meta` files on disk; instead, their GUIDs and names will be stored in the parent FBX's `.meta` file under `importSettings["subAssets"]`.
- Update `loadMetaFile()` to parse `subAssets` from the JSON and register them in `m_metadata` with a `relativePath` like `Models/Char.fbx/Idle.anim`.
- Add getters for `getAnimationClip(AssetGuid guid)` which looks up the virtual asset's parent FBX, retrieves the `ImportedSkeletalMesh`, and returns the correct `AnimationClip` from the vector.

---

### Asset Import Pipeline

#### [MODIFY] [AssetImportPipeline.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetImportPipeline.cpp)
- During `applyImportResult()`, if the imported mesh contains `AnimationClip`s, automatically generate (or preserve) GUIDs for each clip.
- Store these sub-asset GUIDs in the FBX's `AssetMetadata::importSettings` under a `subAssets` JSON dictionary (e.g., `{"Idle": "guid1", "Walk": "guid2"}`).
- Call `updateMetadata()` on the FBX so `AssetDatabase` writes this to the `.meta` file and registers the virtual assets.

---

### Asset Browser UI

#### [MODIFY] [AssetBrowserPanel.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/AssetBrowserPanel.h)
#### [MODIFY] [AssetBrowserPanel.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/AssetBrowserPanel.cpp)
- Introduce "Directory Navigation". `m_currentFolder` can now point to an FBX file (e.g., `guid:<fbx_guid>`).
- If an FBX is double-clicked, update `m_currentFolder` to navigate inside it.
- Render a "Back" button (`<..`) at the top of the grid if we are inside an FBX.
- When inside an FBX, query the `subAssets` from its metadata and render them as individual draggable items (e.g., using an Animation icon/color).
- Users can drag these sub-asset GUIDs into the `PropertiesPanel`.

---

### Humanoid and Properties Panel

#### [MODIFY] [PropertiesPanel.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/PropertiesPanel.cpp)
- Ensure the drag-and-drop payload `ASSET_GUID` works seamlessly for Virtual Assets (it already should, as they will exist in `AssetDatabase::m_metadata`).

## Verification Plan

### Automated Tests
- `AnimationTests.cpp` içerisine yeni testler eklenecek:
  - `TestAnimationBlending`: İki animasyonun (A ve B) 0.5f ağırlıkta %50 - %50 (Lerp & Slerp) birleştirildiğini doğrulayacak.
  - `TestBoneMasking`: Belirli bir kemik maskelendiğinde (Örn: "LeftArm"), diğer kemiklerin A animasyonunda kalırken, sol kolun B animasyonunun matrislerine uyduğunu doğrulayacak.

### Manual Verification
1. Import an FBX file with skeletal animations (e.g., `character.fbx` containing "Idle" and "Walk" clips).
2. Open the Editor, navigate to the Asset Browser, and double-click `character.fbx`.
3. Verify that the browser enters the FBX and displays the sub-assets ("Idle", "Walk", "Mesh").
4. Drag and drop the "Idle" animation sub-asset onto a `Humanoid`'s `idleAnimation` property in the Properties Panel.
5. Verify the property accepts the GUID successfully.
- Bir `Humanoid` nesnesi yaratılıp; "Yürüme" animasyonu oynatılırken, sadece üst gövdeye maskelenmiş "El Sallama" animasyonunun (`Priority`'si daha yüksek olarak) oynatılıp oynatılamadığı kontrol edilecek. Alt gövde yürümeye devam etmeli, üst gövde el sallamalıdır.
