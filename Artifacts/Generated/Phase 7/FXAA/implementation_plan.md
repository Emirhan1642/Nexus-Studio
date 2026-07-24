# FXAA (Fast Approximate Anti-Aliasing) Uygulama Planı

Bu aşamada Post-Processing zincirinin sonuna FXAA (Fast Approximate Anti-Aliasing) eklenecektir. Mevcut durumda motor, HDR görüntüsünü `fs_tonemap.sc` (Tonemapping) üzerinden doğrudan ekrana (Backbuffer) çizmektedir. FXAA eklemek için Tonemapping işlemini ara bir FrameBuffer'a alıp, FXAA shader'ını kullanarak ekrana (Backbuffer) çizeceğiz.

## Önerilen Değişiklikler

### 1. Render Pass Mimarisi
- `View_Tonemap` ID'si, artık doğrudan ekrana değil, yeni oluşturulacak `m_tonemapFB` (SDR - Standard Dynamic Range) isimli FrameBuffer'a çizecek.
- `View_FXAA` adında yeni bir ViewId tanımlanacak.
- `View_FXAA`, `m_tonemapFB`'nin içerisindeki görüntüyü alarak FXAA shader'ından geçirecek ve varsayılan arka belleğe (Backbuffer / Ekran) çizecek.

### 2. C++ Değişiklikleri (`Renderer.h` ve `Renderer.cpp`)
#### [MODIFY] `Renderer.h`
- Yeni `View_FXAA` enum değeri eklenecek.
- Yeni `bgfx::FrameBufferHandle m_tonemapFB` eklenecek.
- Yeni `bgfx::ProgramHandle m_fxaaProgram` eklenecek.
- FXAA ayarları için (Ekran çözünürlüğü, keskinlik vs.) `bgfx::UniformHandle u_fxaaParams` eklenecek.

#### [MODIFY] `Renderer.cpp`
- **Init/Resize:** Ekran çözünürlüğünde (SDR, RGBA8 formatında) `m_tonemapFB` oluşturulacak. Ekran boyutu değiştiğinde güncellenecek.
- **RenderFrame:** Post-Processing zinciri güncellenecek: `Tonemapping -> m_tonemapFB` ve `FXAA -> Ekrana (Backbuffer)`.
- **Shutdown:** Yeni oluşturulan handle'lar yok edilecek.

### 3. Shaderlar
#### [NEW] `fs_fxaa.sc`
- Standart FXAA algoritmasını içerecek olan BGFX uyumlu shader yazılacak. Görüntüyü analiz edip kenar (Luma temelli) tespiti ve yumuşatması yapacak.

#### [MODIFY] `compile_shaders.bat`
- Yeni `fs_fxaa.sc` shader'ını derleme komutlarına eklenecek. (Vertex shader olarak standart `vs_fullscreen.sc` kullanılabilir).

---

> [!IMPORTANT]
> **Açık Soru (Onay Bekliyor):**
> FXAA algoritmasında kullanılacak `EdgeThreshold` (Kenar hassasiyeti) değerini başlangıçta MVP olarak sabit bırakmayı öneriyorum. Gelecekte UI üzerinden değiştirilebilir yapmak istersek, bunu bir sonraki fazda veya UI entegrasyonu aşamasında ekleyebiliriz. Şu an için sadece motor içerisine entegrasyonunu yapmak uygun mudur?
