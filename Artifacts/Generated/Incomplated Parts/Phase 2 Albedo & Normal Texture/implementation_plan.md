# Faz 2: Albedo & Normal Map Doku (Texture) Entegrasyonu

Bu plan, Faz 2 dökümanında (Faz2_Render_Pipeline.md) belirtilen ancak eksik bırakılan doku (texture) yükleme ve PBR materyal özelliklerinin motor içine tam anlamıyla entegre edilmesini hedefler.

## User Review Required

> [!IMPORTANT]
> Dökümanda belirtilen `MaterialData` yapısını ekleyeceğiz. Mevcut durumda `Part` sınıfında sadece enum bazlı basit bir renklendirme (Material) vardı. Yeni sistemde `Part` artık `albedoTexture` ve `normalTexture` dosya yollarını (string olarak) saklayacak. Texture yükleme işlemi BGFX'in `example-common` kütüphanesindeki `loadTexture` fonksiyonu üzerinden yapılacak.

## Open Questions

> [!NOTE]
> 1. Properties panelinde dokuları atamak için bir dosya seçici (File Browser) dialog kutusu mu açalım, yoksa şimdilik sadece dosya yolunu metin olarak girmek (InputText) yeterli mi?
> 2. PBR için Metallic ve Roughness değerlerini Texture map olarak desteklemeye gerek var mı, yoksa dökümanda yazdığı gibi sadece float değerler (uniform) olarak kalmaları yeterli mi? (Dökümanda "float metallic", "float roughness" olarak geçiyor).

## Proposed Changes

---

### Veri Modeli ve Materyal Sistemi (DataModel & Material)

#### [MODIFY] `Engine/Core/DataModel/Part.h` ve `Part.cpp`
- `std::string albedoTexturePath` ve `std::string normalTexturePath` özellikleri eklenecek.
- `Vector3 albedoColor`, `float metallic`, `float roughness`, `float emissiveStrength` alanları eklenecek.
- `ClassBuilder` (Reflection) tarafında bu yeni özellikler `Property` olarak Editor Properties paneline açılacak.

#### [NEW] `Engine/Renderer/Materials/Material.h`
- Dökümandaki `MaterialData` yapısı tanımlanacak:
```cpp
struct MaterialData {
    Engine::Math::Vector3 albedo{1,1,1};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissiveStrength = 0.0f;
    bgfx::TextureHandle albedoTexture = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normalTexture = BGFX_INVALID_HANDLE;
};
```

---

### Renderer ve Shader Güncellemeleri (Rendering)

#### [MODIFY] `Engine/Renderer/SceneGraph/RenderProxy.h`
- `RenderProxy` yapısına `MaterialData material;` alanı eklenecek.

#### [MODIFY] `Engine/Renderer/Renderer.h` ve `Renderer.cpp`
- `RendererSystem` içerisine texture sampler'lar eklenecek:
  - `bgfx::UniformHandle s_texColor;`
  - `bgfx::UniformHandle s_texNormal;`
- Vertex formatı (`PosColorVertex`) güncellenerek UV (TexCoord) `a_texcoord0` desteği eklenecek (Küp vertex dizisi buna göre genişletilecek).
- `bimg/bgfx_utils.h` kullanılarak Texture loading mekanizması eklenecek. Path değiştikçe `loadTexture` çağrılacak.

#### [MODIFY] `Engine/Renderer/Shaders/vs_pbr.sc`
- `$input a_texcoord0` ve `$output v_texcoord0` eklenecek.
- Vertex'ten gelen UV koordinatı fragment shader'a paslanacak.

#### [MODIFY] `Engine/Renderer/Shaders/fs_pbr.sc`
- `SAMPLER2D(s_texColor, 0);` ve `SAMPLER2D(s_texNormal, 1);` eklenecek.
- Eğer texture yüklendiyse `texture2D(s_texColor, v_texcoord0)` ile renk okunacak ve uniform `u_albedoRoughness.xyz` ile çarpılacak (Dökümanda gösterildiği üzere PBR formülüne yedirilecek).

## Verification Plan

### Automated Tests
- `cmake --build build --config Debug --target NexusStudioEditor` çalıştırılarak yeni Vertex düzeninin ve Shader'ların derlendiği kontrol edilecek.

### Manual Verification
1. Editor açılacak, sahneye bir Küp (Part) eklenecek.
2. Properties panelinden `albedoTexturePath` kısmına sistemde mevcut olan bir `.png` veya `.jpg` (örneğin bir tuğla kaplaması) dosyasının tam yolu girilecek.
3. Küpün üzerindeki rengin/kaplamanın bu dosyaya göre güncellenip güncellenmediği gözlemlenecek.
4. Normal map atanarak ışığa göre tepki verip vermediği kontrol edilecek.
