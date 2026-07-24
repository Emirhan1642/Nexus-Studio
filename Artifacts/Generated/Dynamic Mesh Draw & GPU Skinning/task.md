# Task: Dinamik Mesh Çizimi ve GPU Skinning

- `[x]` **Görev 1: SkinnedVertex & MeshHandle Altyapısı**
  - `[x]` `Renderer.h` içerisine `SkinnedVertex` yapısı, bgfx::VertexLayout init eklenecek.
  - `[x]` `MeshHandle` typedef ve `MeshData` (vbh, ibh tutan struct) oluşturulacak.
  - `[x]` `RendererSystem` içerisine `std::unordered_map<MeshHandle, MeshData>` ve `u_boneTransforms` (Uniform Handle) eklenecek.
  
- `[x]` **Görev 2: Mesh Yükleme Fonksiyonu (`loadMesh`)**
  - `[x]` `Renderer.cpp` içerisinde AssetDatabase'den GUID ile `ImportedSkeletalMesh` çekilip `SkinnedVertex` array'e çevrilecek ve VBH/IBH oluşturulacak.
  
- `[x]` **Görev 3: Shader Güncellemesi (GPU Skinning)**
  - `[x]` `vs_pbr.sc` vertex shader dosyasına `a_indices` ve `a_weight` attributeları eklenecek.
  - `[x]` `u_boneTransforms` uniform dizisinden (mat4) matrixler çekilip vertex pos ve normal bükülecek.
  - `[x]` Shader derlenecek (CMake üzerinden `shaderc` ile veya manuel).
  
- `[x]` **Görev 4: Part & RenderProxy Güncellemesi**
  - `[x]` `Part.cpp` içindeki `markRenderDirty` fonksiyonunda, `meshAssetGuid` doluysa `RendererSystem::instance().getMeshHandle(guid)` ile MeshHandle alınacak.
  - `[x]` `RenderProxy` yapısında `std::vector<Math::Matrix4> boneTransforms` (kemik matrisleri) taşınması sağlanacak.
  
- `[x]` **Görev 5: Humanoid / AnimationPlayer Güncellemesi**
  - `[x]` `Humanoid::update` içinde iskelet hesaplaması yapılacak.
  - `[x]` Hesaplanan kemik matrisleri, karakterin `RootPart`'ı üzerindeki proxy `boneTransforms`'ına yazılacak.
  
- `[x]` **Görev 6: Derleme ve Test**
  - `[x]` Proje derlenecek.
  - `[x]` Editörde model sürükle-bırak ile test edilecek.
