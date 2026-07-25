# TAA (Temporal Anti-Aliasing) İş Listesi

- `[x]` **1. C++ Tarafı (`Renderer.h`)**
  - `[x]` `View_TAA = 35` enum'a eklenecek.
  - `[x]` `m_taaFB[2]` ve `m_taaIndex = 0` değişkenleri eklenecek.
  - `[x]` `m_taaProgram` eklenecek.
  - `[x]` `u_taaParams` uniform'u eklenecek.

- `[x]` **2. Halton Jitter ve C++ Mantığı (`Renderer.cpp`)**
  - `[x]` Halton dizisi (2 ve 3 tabanlı) oluşturularak her karede alt-piksel Jitter (titretme) değeri hesaplanacak.
  - `[x]` Ana kamera projeksiyon matrisine Jitter eklenecek (`View_MainColor`).
  - `[x]` Post-Process view'ları (SSGI, SSR, vb.) için Unjittered projeksiyon matrisi set edilecek.
  - `[x]` `m_taaFB` buffer'ları oluşturulacak ve yok edilecek.
  - `[x]` `renderFrame` metodunda TAA pass'i çalıştırılacak ve Ping-Pong (history okuma, yeniye yazma) mantığı kurulacak.

- `[x]` **3. TAA Shader'ı (`fs_taa.sc`)**
  - `[x]` Girdiler: Current Color, History Color, Depth.
  - `[x]` Velocity hesaplaması: `v_texcoord0` ve `s_texDepth` ile dünya pozisyonu bulunup `u_prevViewProj` ile önceki ekran pozisyonu bulunacak.
  - `[x]` 3x3 Neighborhood (komşuluk) Clamping işlemi yapılacak (Ghosting'i önlemek için).
  - `[x]` Jitter dikkate alınarak History ve Current renkleri blend edilecek.

- `[x]` **4. Derleme ve Test**
  - `[x]` `compile_shaders.bat` güncellenecek ve derlenecek.
  - `[x]` Motor derlenip çalıştırılarak TAA ve FXAA ikilisi test edilecek.
