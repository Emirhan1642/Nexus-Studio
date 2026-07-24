# Aşama 5 Devamı: Skeletal Animation Sistemi (İskelet ve Animasyon) Uygulama Planı

Bu plan, `Skeletal_Animation_Sistemi.md` dokümanındaki gereksinimler doğrultusunda, karakterlerin animasyonlarını `.fbx` üzerinden içe aktarmamızı ve GPU üzerinde gerçek zamanlı deri deformasyonu (skinning) ile hareket etmelerini sağlayacak sistemleri kurmayı hedefler.

## User Review Required

> [!IMPORTANT]
> - **Assimp Kütüphanesi:** FBX ve benzeri karmaşık animasyon formatlarını ayrıştırmak için projeye `Assimp` kütüphanesini `FetchContent` ile dahil edeceğim.
> - **Shader Değişikliği:** bgfx üzerinde GPU skinning yapabilmek için mevcut PBR shaderlarına ek olarak `vs_skinned_pbr.sc` shader'ını oluşturup, maksimum `MAX_BONES` (örn: 64) destekleyen bir uniform buffer sistemi entegre edeceğim.
> - **Two-Bone IK (Ters Kinematik):** Ayakların eğimli zemine düzgün oturmasını sağlayacak İki-Kemikli IK matematiğini entegre edeceğim. IK işlemi animasyondan sonra uygulanacak.

## Open Questions

> [!WARNING]
> - Karakterin `AnimationTrack` entegrasyonu (Play/Stop kontrolü) Luau script tarafına bağlanmadan sadece C++ üzerinden test edilse şimdilik yeterli midir? 
> - Maksimum desteklenecek kemik (bone) sayısını `64` olarak kısıtlamam performansı iyileştirir (uniform buffer kısıtı). Bu sınır yeterli midir?

## Proposed Changes

---

### [ThirdParty Dependencies]

`Assimp` kütüphanesini projeye dahil etmek için `ThirdParty/CMakeLists.txt` dosyası düzenlenecek.
#### [MODIFY] [CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/ThirdParty/CMakeLists.txt)

---

### [Engine / Animation]

Skeletal animasyonun çekirdek matematik ve interpolasyon sınıfları oluşturulacak.

#### [NEW] [Skeleton.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/Skeleton.h)
Bone yapısı, hiyerarşi listesi ve `computeWorldTransforms` matematiği.
#### [NEW] [Skeleton.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/Skeleton.cpp)

#### [NEW] [AnimationClip.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationClip.h)
`BoneKeyframes` yapısı. Pozisyon interpolasyonu (`lerp`) ve rotasyon interpolasyonu (`slerp`).
#### [NEW] [AnimationClip.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationClip.cpp)

#### [NEW] [AnimationPlayer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationPlayer.h)
Farklı animasyonlar (örn: `Idle` -> `Walk`) arasındaki geçişleri ve ağırlık karıştırmalarını (crossfade / blend) yönetecek sistem.
#### [NEW] [AnimationPlayer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationPlayer.cpp)

---

### [Engine / Animation / IK]

Eğimli yüzeylerde karakterin ayaklarının zemine doğru basmasını sağlayan Ters Kinematik.

#### [NEW] [TwoBoneIK.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/IK/TwoBoneIK.h)
`solveTwoBoneIK` trigonometri hesaplamaları (Kosinüs teoremi ile).

---

### [Engine / Assets / Importers]

`Assimp` kullanılarak dışarıdan .fbx model, iskelet ve animasyon dosyalarının okunması.

#### [NEW] [SkeletalMeshImporter.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/Importers/SkeletalMeshImporter.h)
#### [NEW] [SkeletalMeshImporter.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/Importers/SkeletalMeshImporter.cpp)

---

### [Engine / Renderer / Shaders]

Skinned (derili) modeller için yeni bir vertex shader oluşturularak bgfx `compile_shaders.bat` dosyasına eklenecek. GPU'ya kemik ağırlıkları (`a_weight`, `a_indices`) aktarılacak.

#### [NEW] [vs_skinned_pbr.sc](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Shaders/vs_skinned_pbr.sc)
#### [MODIFY] [compile_shaders.bat](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Shaders/compile_shaders.bat)

---

### [Engine / Core / DataModel]

Karakter kontrolcüsüne Skeletal Animation ve IK yeteneğinin eklenmesi.

#### [MODIFY] [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.h)
#### [MODIFY] [Humanoid.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.cpp)
`AnimationPlayer` ve `IK` mantığı `Humanoid` sınıfı içerisinde `update()` esnasında uygulanacak.

---

## Verification Plan

### Automated Tests
- `NexusStudioTests.exe` içerisine `AnimationTest` adlı yeni bir test eklenecek.
- Bu testte, sanal bir `AnimationClip` oluşturulacak, `slerp` ve `lerp` hesaplamalarının doğru yapıldığı doğrulanacak.
- `TwoBoneIK` matematiğinin doğruluğu test edilecek.

### Manual Verification
- C++ kodu, MSVC derleyicisi ile hatasız bir şekilde derlenecek.
- Tüm `CMakeLists.txt` dosyaları (EngineAnimation eklenecek) doğru konfigüre edilmiş olacak.
- (Eğer uygun test FBX objesi sağlanabilirse) Assimp'in okuma mekanizmasının çökmediğinden emin olunacak.
