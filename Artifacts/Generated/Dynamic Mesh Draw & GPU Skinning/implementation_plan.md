# Uygulama Planı: Dinamik Mesh Çizimi ve GPU Skinning

Önceki aşamada tespit ettiğimiz en büyük eksiklik olan "Karakterlerin (Skeletal Mesh) ve genel model objelerinin render edilmemesi (sadece sabit küp çizilmesi)" sorununu çözmek için `RendererSystem` üzerinde kapsamlı bir güncelleme yapacağız. Bu güncelleme ile hem normal statik mesh'ler hem de iskelet (skeleton) animasyonuna sahip karakterler ekrana çizilebilecek.

## User Review Required

> [!IMPORTANT]
> Bu aşama, oyun motorunun görsel çıktısını temelden değiştirecek ve GPU Shader katmanına dokunacaktır. 
> BGFX tabanlı Renderer sistemimiz tamamen statik bir yapıdan, dinamik VBO/IBO (Vertex/Index Buffer) yöneten bir Mesh Manager yapısına geçecektir. Lütfen onaylamadan önce teknik detaylara göz atın.

## Open Questions

> [!WARNING]
> GPU Skinning için BGFX tarafında kemik (bone) matrislerini Uniform dizisi olarak (örn. maksimum 64 veya 128 kemik sınırı ile) göndermeyi planlıyorum. `vs_pbr.sc` (Vertex Shader) bu kemik matrislerini alıp ağırlıklara (`a_weight`) göre Vertex'in pozisyonunu bükecektir. MVP (Minimum Viable Product) olduğu için tek geçişli bir skinning (max 4 weights per vertex) sizin için uygun mudur?

## Proposed Changes

### [MODIFY] `Engine/Renderer/Renderer.h` & `Renderer.cpp`
- **MeshHandle Yönetimi:** Sadece sabit `m_vbh` (küp) yerine, bir `std::unordered_map<MeshHandle, MeshData>` yapısı kurulacak.
- **MeshData Yapısı:** Her mesh için `bgfx::VertexBufferHandle` ve `bgfx::IndexBufferHandle` tutulacak.
- **SkinnedVertex Desteği:** 
  ```cpp
  struct SkinnedVertex {
      float x, y, z;
      float nx, ny, nz;
      float u, v;
      uint8_t boneIndices[4];
      float boneWeights[4];
  };
  ```
  Bu yapı için `bgfx::VertexLayout` (Vertex Declarations) eklenecek.
- **loadMesh() API:** `AssetGuid` üzerinden `AssetDatabase`'den çekilen `ImportedSkeletalMesh`'leri, `RendererSystem` içerisine atıp `MeshHandle` almamızı sağlayacak bir metot yazılacak.
- **Bone Transform Uniforms:** `u_boneTransforms` (örneğin 64x Matrix4) bgfx Uniform'u eklenecek.
- `renderFrame` döngüsü tüm `RenderProxy` objelerini gezerken `proxy.mesh` (MeshHandle) değerine göre doğru VBO/IBO'yu bind edecek. Animasyonlu objeler için o karenin (frame) kemik matrislerini gönderecek.

### [MODIFY] `Engine/Core/DataModel/Part.cpp`
- `markRenderDirty` içerisinde, eğer obje bir `meshAssetGuid`'e sahipse, `RendererSystem::instance().getMeshHandle(meshAssetGuid)` çağrısı ile dönen `MeshHandle` değeri proxy'e set edilecek. Böylece Part objesi küp yerine asıl modelini çizecek.

### [MODIFY] `Engine/Core/DataModel/Humanoid.cpp`
- `Humanoid` sınıfı her tick (kare) güncellenirken `AnimationPlayer` üzerinden iskeletin yerel pozlarını hesaplar (Zaten çalışıyor).
- Hesaplanan matris dizisi `RenderProxy` içine (veya doğrudan Renderer'a) aktarılarak, modelin vertex'lerinin doğru kıvrılması sağlanacak.

### [MODIFY] Orijinal Shader Dosyaları (vs_pbr.sc)
- `a_indices` (uint8) ve `a_weight` (float) verileri eklenecek.
- Skinning formülü:
  ```glsl
  mat4 boneTransform = u_boneTransforms[a_indices[0]] * a_weight[0] +
                       u_boneTransforms[a_indices[1]] * a_weight[1] + ...
  vec4 skinnedPos = mul(boneTransform, vec4(a_position, 1.0));
  ```

## Verification Plan

### Manuel Doğrulama
1. Sahneye Asset Browser üzerinden `character.fbx` dosyası sürüklenecek.
2. Eskiden olduğu gibi görünmez kalmak yerine, karakter modeli sahne üzerinde tüm PBR materyal özellikleri ve doğru boyutlarıyla yer alacak.
3. Skeletal Animation'un test edilebilmesi için karakterin oynatılmakta olan Idle veya Walk animasyonuna göre kollarının/bacaklarının hareket ettiği (ve editör kamerasıyla etrafında dönüldüğünde bunun göze çarptığı) teyit edilecek.
