# Cascaded Shadow Maps (CSM) İş Listesi

- `[x]` **1. Veri Yapıları ve Uniformlar**
  - `[x]` `Renderer.h` içerisine 3 adet Shadow Map FrameBuffer tanımlanması (`m_shadowMapFBs[3]`).
  - `[x]` 3 farklı sampler tanımlanması (`s_texShadow0`, `s_texShadow1`, `s_texShadow2`).
  - `[x]` Cascade mesafeleri için `u_csmParams` (Vec4) uniform'unun tanımlanması.
  - `[x]` 3 adet ışık matrisi için `u_lightMtx0`, `u_lightMtx1`, `u_lightMtx2` tanımlanması (BGFX array uniform'u kullanılarak tek bir `u_lightMtx` olarak da tutulabilir).

- `[x]` **2. Cascade Matris Hesaplaması**
  - `[x]` `Renderer.cpp` içerisine kameranın frustum'unu derinliğe göre bölen ve ışık ortografik matrislerini hesaplayan lojiğin eklenmesi.
  - `[x]` Bölünme mesafeleri: Örn. 0-15m, 15-50m, 50-150m.

- `[x]` **3. Multi-pass Gölge Render**
  - `[x]` Mevcut `View_ShadowPass`'in 3 ayrı `ViewId` üzerinden (Örn: 0, 1, 2) tekrar edilmesi.
  - `[x]` Her pass için uygun `m_shadowMapFBs[i]`'ye ve matrisine bağlanması.

- `[x]` **4. Shader Güncellemeleri**
  - `[x]` `varying.def.sc`: Gerekirse `v_viewDepth` eklenmesi.
  - `[x]` `vs_pbr.sc`: Varsa yeni varying'lerin atanması. (Gölge matrisi hesaplamaları Fragment shader'da `u_lightMtx` dizisi kullanılarak yapılabilir).
  - `[x]` `fs_pbr.sc`: `v_viewDepth` değerine göre doğru `s_texShadowX` ve `u_lightMtx` seçilerek PCF hesaplanması.
  - `[x]` `compile_shaders.bat` çalıştırılarak derlenmesi.

- `[x]` **5. Derleme ve Test**
  - `[x]` Hataların giderilmesi ve uygulamanın pürüzsüz gölgelerle çalıştırılması.
