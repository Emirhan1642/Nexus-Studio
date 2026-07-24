# Faz 7.1 (MVP) - Post-Processing Pipeline (Bloom & Tonemap)

Bu plan, **Faz 7: Gelişmiş Grafik** aşamasının MVP (7.1) sürümü için en kritik başlangıç noktası olan **Post-Processing (Son İşlem)** altyapısının kurulmasını hedeflemektedir. 

## Hedef
Oyun motoruna profesyonel bir görünüm katacak olan "Ping-pong Framebuffer" mimarisini kurmak ve bu mimari üzerinden **Tonemapping (ACES)** ve **Bloom (Parlama)** efektlerini uygulamak.

## Neden Sadece Post-Processing ile Başlıyoruz?
Faz 7 dökümanında SSGI, LOD, MeshOptimizer ve Node-tabanlı Materyal Editörü gibi devasa sistemler bulunuyor. Tüm bu sistemleri aynı anda entegre etmek mimari karmaşaya yol açar. Görüntü kalitesini anında arşa çıkaracak olan temel sistem **Post-Processing**'dir. Önce bu altyapıyı kurup ardından LOD veya Node-Editor sistemine geçmek en güvenli yöntemdir.

---

> [!IMPORTANT]
> **User Review Required:**
> Bu plan sadece Post-Processing (Bloom + Tonemap) altyapısına odaklanmaktadır. SSGI, VCT veya LOD sistemlerini bir sonraki plana saklıyorum. Bu sıralama sizin için uygun mu?

---

## Önerilen Değişiklikler

### 1. Post-Processing Mimarisinin (CPU) Kurulması
`RendererSystem` içerisine `PostProcessPipeline` mantığı eklenecek.
- Sahnemiz artık doğrudan ekrana (Backbuffer'a) değil, ara bir **HDR Framebuffer**'a (RGBA16F) çizilecek.
- HDR görüntü, Post-Process aşamalarından geçip en son adımdaki Tonemap ile LDR'ye (Ekrana uygun formata) çevrilecek.

#### [MODIFY] `Engine/Renderer/Renderer.h`
- `m_hdrFB` (HDR çizim hedefi) eklenecek.
- Ping-pong blur işlemleri için çözünürlüğü düşürülmüş (Downscaled) Framebuffer'lar tanımlanacak.
- Bloom ve Tonemap shader'ları için yeni bgfx program handle'ları eklenecek.

#### [MODIFY] `Engine/Renderer/Renderer.cpp`
- `init()` içerisinde gerekli HDR Framebuffer'lar oluşturulacak.
- `renderFrame()` metoduna **Pass 3: Post-Processing** eklenecek.
- Çizimler artık doğrudan ekrana (View_MainColor) değil, HDR hedefe yapılacak.

### 2. Shader Geliştirmeleri (GPU)
BGFX Shaderc kullanılarak yeni post-process shader'ları yazılacak. Tam ekran (Fullscreen Quad) çizimi için özel bir Vertex Shader kullanılacak.

#### [NEW] `Engine/Renderer/Shaders/vs_fullscreen.sc`
- Ekrana tam oturan bir üçgen çizecek standart vertex shader.

#### [NEW] `Engine/Renderer/Shaders/fs_bloom_threshold.sc`
- HDR görüntüdeki sadece belli bir parlaklık eşiğinin (Threshold) üzerindeki pikselleri ayıklayacak.

#### [NEW] `Engine/Renderer/Shaders/fs_bloom_blur.sc`
- Çıkarılan parlak bölgeleri yatay ve dikey eksende bulanıklaştıracak (Gauss veya Dual-Filtering Blur).

#### [NEW] `Engine/Renderer/Shaders/fs_tonemap.sc`
- Bulanıklaştırılmış Bloom görüntüsünü orijinal HDR görüntüyle toplayacak (Additive Blend).
- ACES Filmic tonemapping uygulayarak renkleri patlamadan monitör formatına dönüştürecek.

#### [MODIFY] `Engine/Renderer/Shaders/compile_shaders.bat`
- Yeni eklenen `vs_fullscreen`, `fs_bloom_threshold`, `fs_bloom_blur` ve `fs_tonemap` shader'ları derleme listesine eklenecek.

---

## Doğrulama Planı (Verification)
1. **Derleme:** `compile_shaders.bat` çalıştırılıp hata olup olmadığına bakılacak. CMake ile `NexusStudioEditor.exe` derlenecek.
2. **Test:** Editör açılacak. Sahnede yüksek Emissive (Parlama) değerine sahip yeni bir obje oluşturulacak.
3. **Görsel Kontrol:** Emissive objenin etrafında sinematik bir parlama (Bloom) olduğu ve aşırı parlak renklerin (Tonemapping sayesinde) beyazlaşmak yerine doğru bir şekilde degrade olduğu (renk koruması) gözlemlenecek.

Lütfen bu planı onaylayın, hemen kodlamaya geçelim!
