# FXAA Entegrasyonu İş Listesi

- `[x]` **1. Veri Yapıları ve Uniformlar (`Renderer.h`)**
  - `[x]` `RenderView` içerisine `View_FXAA = 31` eklenecek.
  - `[x]` `bgfx::FrameBufferHandle m_tonemapFB` eklenecek.
  - `[x]` `bgfx::ProgramHandle m_fxaaProgram` eklenecek.
  - `[x]` `bgfx::UniformHandle u_fxaaParams` ve `s_texTonemap` eklenecek.

- `[x]` **2. Init, Resize ve Shutdown Güncellemeleri (`Renderer.cpp`)**
  - `[x]` `init` metodunda `m_fxaaProgram`, `u_fxaaParams` ve `s_texTonemap` oluşturulacak.
  - `[x]` Ekran boyutu değiştiğinde (Resize) `m_tonemapFB` SDR (RGBA8 formatında) oluşturulacak.
  - `[x]` `shutdown` metodunda tüm bu yeni nesneler (`m_tonemapFB`, `m_fxaaProgram`, uniform'lar) yok edilecek.

- `[x]` **3. Render Pipeline Güncellemesi (`Renderer.cpp`)**
  - `[x]` `View_Tonemap` ID'sinin Framebuffer hedefi `m_tonemapFB` olarak ayarlanacak.
  - `[x]` Render döngüsünün en sonuna `View_FXAA` pass'i eklenecek. Bu pass, ekrana çizecek (BGFX_INVALID_HANDLE) ve `m_tonemapFB` içerisindeki color buffer'ı girdi olarak alacak.

- `[x]` **4. Shader Dosyalarının Oluşturulması**
  - `[x]` `Engine/Renderer/Shaders/fs_fxaa.sc` oluşturulup Luma tabanlı FXAA algoritması yazılacak.
  - `[x]` `compile_shaders.bat` dosyasına `fs_fxaa.sc` komutu eklenecek.

- `[x]` **5. Derleme ve Test**
  - `[x]` `compile_shaders.bat` çalıştırılarak yeni shader derlenecek.
  - `[x]` C++ kodu CMake ile derlenip test edilecek.
