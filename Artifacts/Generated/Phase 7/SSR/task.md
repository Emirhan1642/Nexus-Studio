# SSR (Screen Space Reflections) İş Listesi

- `[x]` **1. G-Buffer Güncellemesi (`fs_pbr.sc`)**
  - `[x]` Normal haritasının (`gl_FragData[1]`) Alpha kanalına `roughness` değeri yazılacak.

- `[x]` **2. C++ Tarafı (`Renderer.h` & `Renderer.cpp`)**
  - `[x]` `Renderer.h` içerisinde `View_SSR = 34` tanımlanacak.
  - `[x]` `m_ssrFB` ve `m_ssrProgram` eklenecek.
  - `[x]` `u_ssrParams` uniform'u (Vec4) eklenecek.
  - `[x]` `Renderer.cpp` init, shutdown ve resize adımlarında bu değişkenlerin yönetimi sağlanacak.
  - `[x]` `renderFrame` metodunda, SSGI sonrası SSR pass'i çalıştırılacak ve DOF pass'inin girdisi `m_ssrFB` olarak güncellenecek.

- `[x]` **3. SSR Shader Oluşturulması (`fs_ssr.sc`)**
  - `[x]` Yeni shader dosyası eklenecek.
  - `[x]` Girdiler: SSGI Color, G-Buffer Normal (Alpha=Roughness), Depth.
  - `[x]` Roughness threshold kontrolü eklenecek.
  - `[x]` Ekran uzayı ışın izleme (Raymarching - 20 adım) ile yansıma hesaplanıp Fresnel ve Roughness'a göre Color ile birleştirilecek.

- `[x]` **4. Derleme ve Test**
  - `[x]` `compile_shaders.bat` dosyasına `fs_ssr.sc` eklenecek.
  - `[x]` Shader'lar derlenip motor çalıştırılacak ve yansımalar test edilecek.
