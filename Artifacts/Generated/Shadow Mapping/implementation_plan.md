# Gölgeleme (Shadow Mapping) Uygulama Planı

Mevcut render sisteminde sadece PBR (Albedo/Normal/Roughness/Metallic) çizimi yapılmakta ancak herhangi bir sahne ışığı (Directional Light) veya obje gölgesi bulunmamaktadır. Sahneyi boyutlandırıp gerçekçi kılmak için bgfx üzerinde iki aşamalı (2-pass) bir Shadow Mapping mimarisi kuracağız.

## User Review Required

> [!WARNING]
> Bu aşamada MVP (Minimum Viable Product) yaklaşımı olarak tek bir Güneş Işığı (Directional Light) için standart bir Shadow Map (Gölge Haritası) uygulayacağız. Point Light (Nokta Işıkları) veya Cascaded Shadow Maps (CSM - Uzaklık bazlı kademeli gölge) gibi ileri düzey sistemler şimdilik karmaşıklığı artırmamak adına bu planın dışında tutulmuştur. Bu yaklaşımı onaylıyor musunuz?

## Proposed Changes

### EngineRenderer (C++ Tarafı)

#### [MODIFY] `Engine/Renderer/Renderer.h`
- Gölge haritasını tutacak `bgfx::FrameBufferHandle m_shadowMapFB` eklenecek.
- Işık uzay (Light Space) dönüşüm matrisini shader'lara göndermek için `bgfx::UniformHandle u_lightMtx` (Light View-Projection matrix) eklenecek.
- Gölge dokusunu (Shadow Texture) shader'a bağlamak için `bgfx::UniformHandle s_texShadow` eklenecek.
- `RenderView` enum'u güncellenecek: `View_ShadowPass = 0` ve `View_MainColor = 1`.

#### [MODIFY] `Engine/Renderer/Renderer.cpp`
- **İlklendirme (Init):** Ortalama `2048x2048` boyutunda, formatı `D16` veya `D24` olan bir Depth FrameBuffer oluşturulacak.
- **renderFrame() Güncellemesi:** Mevcut tekli döngü yerine objeler ekrana **iki defa** çizilecek.
  1. **Gölge Geçişi (Shadow Pass):** Kamera yerine Güneş'in (Directional Light) açısından objeler `m_shadowMapFB` hedefine sadece `vs_shadow` (veya skinned shadow) kullanılarak çizilecek.
  2. **Ana Geçiş (Main Pass):** Mevcut çizim döngüsü çalışacak, ancak bu kez `s_texShadow` olarak bir önceki adımda elde edilen derinlik haritası (Shadow Map) ve `u_lightMtx` GPU'ya gönderilecek.

### Shaders (GPU Tarafı)

#### [NEW] `Engine/Renderer/Shaders/vs_shadow.sc` & `vs_skinned_shadow.sc`
- Ekrana renk basmayan, objeleri sadece ışık kamerasından Depth (derinlik) buffer'a yazmak üzere konumlandıran minimal Vertex Shader'lar eklenecek.

#### [NEW] `Engine/Renderer/Shaders/fs_shadow.sc`
- Sadece `bgfx` sisteminin gereksinimlerini karşılayacak boş veya minimal bir Fragment Shader. (Genelde derinlik donanımsal olarak yazılır, pikseller discard edilmiyorsa boş bile bırakılabilir).

#### [MODIFY] `Engine/Renderer/Shaders/varying.def.sc`
- `v_posLightSpace` (Işık uzayındaki konum) değişkeni eklenecek. Ana geçişte (Main Pass) piksellerin gölge haritasındaki karşılığını bulmak için kullanılacak.

#### [MODIFY] `Engine/Renderer/Shaders/vs_pbr.sc` & `vs_skinned_pbr.sc`
- Vertex shader'da her vertex'in pozisyonu, Güneş'in View-Projection matrisi (`u_lightMtx`) ile çarpılarak `v_posLightSpace` değişkenine atanacak.

#### [MODIFY] `Engine/Renderer/Shaders/fs_pbr.sc`
- Işıklandırma (Lighting) matematiğine PCF (Percentage-Closer Filtering) tabanlı yumuşak gölge kontrolü eklenecek. Eğer ilgili piksel gölgede kalıyorsa, ortam (Ambient) ışığı hariç Güneş ışığı (Diffuse/Specular) o piksele etki etmeyecek şekilde karartılacak.
- `compile_shaders.bat` güncellenerek yeni shader'ların derlenmesi sağlanacak.

## Verification Plan

### Manual Verification
1. Editör üzerinden `ShadowMap` aktif edilmiş haliyle proje başlatılacak.
2. Sahnedeki animasyonlu karakterin (`Humanoid`) ve yerleştirilmiş sabit küplerin (`Part`) zemin üzerine doğru açıdan ve doğru koyulukta gölge düşürüp düşürmediği gözlemlenecek.
3. Karakter hareket ettikçe animasyonuna (GPU Skinning) bağlı olarak kollarının ve bacaklarının gölgesinin de gerçek zamanlı hareket ettiği teyit edilecek.
