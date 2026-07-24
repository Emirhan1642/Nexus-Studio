# Task: Gölgeleme (Shadow Mapping)

- `[x]` **Görev 1: bgfx Shader Hazırlıkları (GPU)**
  - `[x]` `Engine/Renderer/Shaders/vs_shadow.sc` oluşturulacak.
  - `[x]` `Engine/Renderer/Shaders/vs_skinned_shadow.sc` oluşturulacak.
  - `[x]` `Engine/Renderer/Shaders/fs_shadow.sc` oluşturulacak.
  - `[x]` `varying.def.sc` içerisine `vec4 v_posLightSpace;` eklenecek.
  - `[x]` `vs_pbr.sc` ve `vs_skinned_pbr.sc` güncellenerek `v_posLightSpace` doldurulacak.
  - `[x]` `fs_pbr.sc` içerisine PCF gölge harmanlaması (shadow map sampling) eklenecek.
  - `[x]` `compile_shaders.bat` dosyasına yeni shader derleme komutları eklenecek.

- `[x]` **Görev 2: Renderer.h Güncellemesi (CPU)**
  - `[x]` `m_shadowMapFB`, `u_lightMtx`, `s_texShadow` gibi bgfx handle'ları eklenecek.
  - `[x]` `View_ShadowPass = 0`, `View_MainColor = 1` enumları düzenlenecek.
  - `[x]` Yeni shadow shader program handle'ları (statik ve skinned) eklenecek.

- `[x]` **Görev 3: Renderer.cpp Güncellemesi (CPU)**
  - `[x]` `init()` fonksiyonunda Shadow FrameBuffer ve dokusu oluşturulacak.
  - `[x]` `init()` içerisinde `u_lightMtx` ve `s_texShadow` uniformları oluşturulacak.
  - `[x]` Shader'lar belleğe yüklenecek.
  - `[x]` `renderFrame()` metoduna Shadow Pass eklenecek.
  - `[x]` Directional Light matris hesaplamaları (Orthographic projeksiyon) yapılacak.
  - `[x]` Çizim (Submit) esnasında Shadow Pass'ten sonra Main Pass'te `s_texShadow` ve `u_lightMtx` bağlanacak.
  - `[x]` Shutdown() ile bellekteki kaynaklar serbest bırakılacak.

- `[x]` **Görev 4: Derleme ve Test**
  - `[x]` Shaders (compile_shaders.bat) ve C++ (CMake) derlenecek.
  - `[x]` Gölgelerin doğru çalıştığı editör üzerinden doğrulanacak.
