# Skeletal Animation Görev Listesi (Aşama 5 Devamı)

- `[x]` **Görev 1: Bağımlılıkların Eklenmesi**
  - `[x]` `ThirdParty/CMakeLists.txt` içerisine `Assimp` FetchContent bloklarını ekle.
  - `[x]` `Engine/CMakeLists.txt` dosyasını `Assimp` kütüphanesini bağlayacak şekilde güncelle.

- `[x]` **Görev 2: Çekirdek Animasyon Sınıfları (Engine/Animation)**
  - `[x]` `Skeleton.h/cpp` (Bone hiyerarşisi, bind pose ve world transform matris hesaplaması).
  - `[x]` `AnimationClip.h/cpp` (Zaman bazlı pozisyon LERP ve rotasyon SLERP enterpolasyonu).
  - `[x]` `AnimationPlayer.h/cpp` (Sürekli güncellenen state, klip geçişlerinde crossfade blend hesabı).

- `[x]` **Görev 3: İleri Fizik & Matematik**
  - `[x]` `TwoBoneIK.h/cpp` (Ayaklar için ters kinematik trigonometri hesabı).

- `[x]` **Görev 4: Import ve Varlık (Asset) İşleme**
  - `[x]` `Engine/Assets/Importers/SkeletalMeshImporter.h/cpp` oluştur.
  - `[x]` Assimp ile `.fbx` verilerini çek (kemik sınırlandırmaları (4 bone limit) ve ağırlık hesabı ile).

- `[x]` **Görev 5: Rendering ve Skinning Shader'ı**
  - `[x]` `Engine/Renderer/Shaders/vs_skinned_pbr.sc` oluştur (u_boneMatrices ve a_weight, a_indices kullanarak CPU'dan GPU'ya skinning matrisini aktar).
  - `[x]` `compile_shaders.bat` dosyasını yeni shader'ı derleyecek şekilde güncelle.

- `[x]` **Görev 6: Engine Entegrasyonu (Humanoid & Luau)**
  - `[x]` `AnimationTrack.h/cpp` (Instance'dan türemiş) sınıfını yaz ve `ClassBuilder` ile Luau tarafına `Play`, `Stop` metodlarını ekle.
  - `[x]` `Humanoid.h/cpp` içerisine `AnimationPlayer` entegrasyonu yap (`update` döngüsünde IK uygulayıp ardından GPU buffer'ına veri gönderecek altyapı).

- `[x]` **Görev 7: Derleme ve Test**
  - `[x]` Tüm C++ kodunu baştan derle ve `NexusStudioTests.exe` içinde `AnimationTest` sınıfı oluşturarak doğrula.
