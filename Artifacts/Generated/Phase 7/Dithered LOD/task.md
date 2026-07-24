# Dithered LOD Transitions İş Listesi

- `[x]` **1. C++ Uniform Yönetimi (`Renderer.h` & `Renderer.cpp`)**
  - `[x]` `bgfx::UniformHandle u_lodParams` eklenecek (x = dither fade).
  - `[x]` `init` ve `shutdown` içerisinde oluşturulup yok edilecek.

- `[x]` **2. LOD Geçiş Mantığı (`Renderer.cpp - renderFrame`)**
  - `[x]` `renderFrame` içerisinde `distSq` (mesafe) kontrolünde geçiş bölgeleri (Transition Zones) belirlenecek.
  - `[x]` Eğer geçiş bölgesinde değilse mevcut LOD draw edilecek (`u_lodParams.x = 1.0`).
  - `[x]` Eğer geçiş bölgesinde ise 2 ayrı Draw Call yapılacak. Birinci LOD `fade` ile, ikinci LOD `1.0 - fade` ile çizilecek.
  - `[x]` Bu mantık Shadow Map pass'inde de çalışacak şekilde ayarlanacak.

- `[x]` **3. Shader Güncellemeleri**
  - `[x]` `Engine/Renderer/Shaders/fs_pbr.sc` shader'ında `u_lodParams` okunacak ve `gl_FragCoord.xy` kullanılarak 4x4 Bayer Dither kontrolü ile `discard` işlemi yapılacak.
  - `[x]` Aynı dither mantığı `Engine/Renderer/Shaders/fs_shadow.sc` içerisine eklenecek.
  - `[x]` `compile_shaders.bat` ile shader'lar derlenecek.

- `[x]` **4. Editor Settings İçin Not Alma**
  - `[x]` Gelecekte Editor Settings eklendiğinde değiştirebilmek üzere, geçiş mesafesi kalınlığı (Örn: 10m) ve Gölge Dither'ı aç-kapa özelliği `eksikler_ve_iyilestirmeler.md` listesine eklenecek.

- `[x]` **5. Derleme ve Test**
  - `[x]` C++ kodu CMake ile derlenip, geçişlerin eriyerek (pop-in olmadan) çalıştığı test edilecek.
