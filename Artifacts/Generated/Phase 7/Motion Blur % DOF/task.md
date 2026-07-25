# DOF & Motion Blur İş Listesi

- `[x]` **1. Veri Yapıları ve Değişkenler (`Renderer.h`)**
  - `[x]` `RenderView` içerisine `View_DOF = 32` ve `View_MotionBlur = 33` eklenecek.
  - `[x]` `bgfx::FrameBufferHandle m_dofFB` ve `m_mbFB` eklenecek.
  - `[x]` `bgfx::ProgramHandle m_dofProgram` ve `m_mbProgram` eklenecek.
  - `[x]` Uniformlar: `u_dofParams`, `u_mbParams`, `u_prevViewProj`, `u_invViewProj` eklenecek.
  - `[x]` `RendererSystem` içerisine `Engine::Math::Matrix4 m_prevViewProj` eklenecek.

- `[x]` **2. Init, Resize ve Shutdown Güncellemeleri (`Renderer.cpp`)**
  - `[x]` `init` metodunda uniformlar ve shader programları yüklenecek.
  - `[x]` Resize işleminde `m_dofFB` ve `m_mbFB` HDR (RGBA16F) olarak oluşturulup yok edilecek.
  - `[x]` `shutdown` metodunda kaynaklar temizlenecek.

- `[x]` **3. Render Pipeline Güncellemesi (`Renderer.cpp - renderFrame`)**
  - `[x]` Güncel `viewProj` ve `invViewProj` matrisleri hesaplanacak.
  - `[x]` **DOF Pass:** Girdi olarak `m_ssgiFB` (color) ve `m_hdrFB` (depth) alıp, sonucu `m_dofFB`'ye çizecek.
  - `[x]` **Motion Blur Pass:** Girdi olarak `m_dofFB` (color) ve `m_hdrFB` (depth) alıp, sonucu `m_mbFB`'ye çizecek.
  - `[x]` **Tonemap Pass:** Mevcut durumda `m_ssgiFB`'yi okuyan kısım, artık `m_mbFB`'yi okuyacak şekilde güncellenecek.
  - `[x]` `renderFrame` sonunda `m_prevViewProj = viewProj` şeklinde saklanacak.

- `[x]` **4. Shader Dosyalarının Oluşturulması**
  - `[x]` `fs_dof.sc`: Derinlik haritasından CoC (Circle of Confusion) hesaplayıp gauss tabanlı blur uygulayan shader yazılacak.
  - `[x]` `fs_mb.sc`: `u_invViewProj` ve `u_prevViewProj` ile hız vektörü hesaplayıp blur uygulayan shader yazılacak.
  - `[x]` `compile_shaders.bat` güncellenerek bu yeni shaderlar derlenecek.

- `[x]` **5. Derleme ve Test**
  - `[x]` Shader'lar derlenip motor başlatılacak ve efektler test edilecek.
