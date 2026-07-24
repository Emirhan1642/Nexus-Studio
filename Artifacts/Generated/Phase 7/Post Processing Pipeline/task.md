# Task: Faz 7.1 Post-Processing (Bloom & Tonemap)

- `[x]` **Görev 1: Shader Altyapısı (GPU)**
  - `[x]` `Engine/Renderer/Shaders/vs_fullscreen.sc` oluşturulacak (Tam ekran çizim).
  - `[x]` `Engine/Renderer/Shaders/fs_bloom_threshold.sc` oluşturulacak.
  - `[x]` `Engine/Renderer/Shaders/fs_bloom_blur.sc` oluşturulacak (Gauss bulanıklaştırma).
  - `[x]` `Engine/Renderer/Shaders/fs_tonemap.sc` oluşturulacak (ACES Filmic & Combine).
  - `[x]` `compile_shaders.bat` dosyasına yeni shader derleme komutları eklenecek.

- `[x]` **Görev 2: Renderer Başlık Dosyası Güncellemesi (Renderer.h)**
  - `[x]` Yeni Uniform'lar (örn. `s_texColor`, `s_texBloom`, `u_bloomParams`) tanımlanacak.
  - `[x]` Shader Program Handle'ları (`m_bloomThresholdProgram`, `m_bloomBlurProgram`, `m_tonemapProgram`) tanımlanacak.
  - `[x]` HDR ve Ping-Pong Blur FrameBuffer'ları için tanımlamalar yapılacak.
  - `[x]` Post-Processing için RenderView ID'leri eklenecek (Örn. `View_PostProcess = 2`).

- `[x]` **Görev 3: Renderer Uygulaması (Renderer.cpp)**
  - `[x]` `init()` içerisinde RGBA16F formatında HDR FrameBuffer oluşturulacak.
  - `[x]` Ping-pong blur FrameBuffer'ları oluşturulacak.
  - `[x]` Yeni post-process shader'ları belleğe yüklenecek.
  - `[x]` `renderFrame()` metoduna Post-Process (Pass 3) eklenecek ve Fullscreen Quad (Tam ekran üçgeni) çizdirilerek post-process zinciri işletilecek.
  - `[x]` `shutdown()` metoduna yeni kaynakların serbest bırakılması eklenecek.

- `[ ]` **Görev 4: Derleme ve Doğrulama**
  - `[ ]` Proje CMake ile derlenecek.
  - `[ ]` Editör üzerinden Bloom ve Tonemapping test edilecek.
