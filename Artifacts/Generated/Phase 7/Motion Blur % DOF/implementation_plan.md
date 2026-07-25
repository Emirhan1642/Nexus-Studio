# Faz 7.3 - Görev 5: Motion Blur & Depth of Field (DOF) Uygulama Planı

Bu aşamada oyun motorunun Post-Processing zincirine iki önemli sinematik efekt eklenecektir: Kamera hareketine dayalı **Motion Blur (Hareket Bulanıklığı)** ve netleme mesafesine dayalı **Depth of Field (Alan Derinliği)**.

## 1. Post-Processing Zincirinin Yeni Durumu
Önceki zincir: `SSGI -> Bloom -> Tonemap -> FXAA`
Yeni zincir: `SSGI -> Bloom (Threshold) -> DOF -> Motion Blur -> Tonemap (Bloom Combine ile birlikte) -> FXAA`

Bu işlem için 2 yeni FrameBuffer oluşturulacaktır:
- `bgfx::FrameBufferHandle m_dofFB` (RGBA16F - HDR)
- `bgfx::FrameBufferHandle m_mbFB` (RGBA16F - HDR)

## 2. C++ (Renderer.h ve Renderer.cpp) Değişiklikleri
- `RenderView` enumuna `View_DOF` ve `View_MotionBlur` eklenecek.
- `init()` ve `resize()` metodlarında `m_dofFB` ve `m_mbFB` (SDR değil HDR formatında `bgfx::TextureFormat::RGBA16F` olarak) üretilecek.
- DOF için `u_dofParams` (Focus Distance, Focal Length, vb.) uniform'u eklenecek.
- Motion Blur için `RendererSystem` içerisine `Engine::Math::Matrix4 m_prevViewProj` saklanacak.
- Motion Blur shader'ı için `u_prevViewProj` ve `u_invViewProj` matris uniform'ları eklenecek.

## 3. Shader Değişiklikleri

### A. fs_dof.sc (Depth of Field)
- **Girdiler:** `s_texColor` (SSGI Çıktısı), `s_texDepth` (Derinlik Haritası).
- **İşlem:** Depth buffer'dan doğrusal derinlik hesaplanır. Odak mesafesine (Focus Distance) göre Circle of Confusion (CoC) hesaplanır. CoC boyutuna göre Poisson disk veya Box/Gaussian blur uygulanır.
- **Çıktı:** `m_dofFB`.

### B. fs_motion_blur.sc (Motion Blur)
- **Girdiler:** `s_texColor` (DOF Çıktısı), `s_texDepth` (Derinlik Haritası).
- **İşlem:**
  1. Pixel'in ekran koordinatı ve Depth Buffer değeri kullanılarak `u_invViewProj` matrisi ile **Dünya Koordinatı (World Position)** hesaplanır.
  2. Bu dünya koordinatı `u_prevViewProj` matrisi ile çarpılarak önceki karedeki ekran koordinatı bulunur.
  3. Güncel koordinat ile önceki koordinat arasındaki fark **Velocity Vector (Hız Vektörü)** olarak belirlenir.
  4. Hız vektörü doğrultusunda (örneğin 8-16 sample alınarak) renkler toplanıp ortalaması alınır.
- **Çıktı:** `m_mbFB`.

### C. Tonemap Güncellemesi
- `View_Tonemap` pass'i artık girdisini `m_ssgiFB`'den değil, `m_mbFB`'den alacaktır.

---

> [!IMPORTANT]
> **Kullanıcı İncelemesi / Açık Sorular:**
> 1. Bu iki pass, Full Screen Quad üzerinde yoğun hesaplama yapacaktır. Depth of Field (DOF) için MVP aşamasında basit ve performanslı bir Gauss tabanlı bulanıklaştırma kullanmayı öneriyorum. Gelişmiş Bokeh efekti (dairesel parlamalar) daha çok işlem gücü gerektirir. Basit DOF uygun mudur?
> 2. Motion Blur için şu an sadece **Kamera Hareketi (Camera Motion Blur)** planlandı. Objelerin kendi hareketleri (Object Motion Blur) için ayrı bir Velocity G-Buffer oluşturulması ve tüm obje shader'larının modifiye edilmesi gerekir. Şu anlık sadece kamera hareketine duyarlı Motion Blur yapmak uygun mudur? (Çoğu AAA oyun da maliyetten dolayı sadece kamera blur'u kullanır veya object blur'u opsiyonel sunar).
