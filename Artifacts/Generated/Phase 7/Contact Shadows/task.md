# Contact Shadows İş Listesi

- `[x]` **1. C++ Uniform Tanımlaması (`Renderer.h` & `Renderer.cpp`)**
  - `[x]` `Renderer.h` içerisine `bgfx::UniformHandle u_lightDir` eklenecek.
  - `[x]` `Renderer.cpp` içerisindeki `init()` metodunda uniform oluşturulacak, `shutdown()` metodunda yok edilecek.

- `[x]` **2. Işık Yönünün Hesaplanması ve Gönderilmesi (`Renderer.cpp - renderFrame`)**
  - `[x]` SSGI pass'ine (Pass 3.1) gelindiğinde, hardcoded `lightDir` (`{0.577f, 0.577f, -0.577f}`) kameranın View matrisi (sadece rotasyon kısmı) ile çarpılarak View-Space'e dönüştürülecek.
  - `[x]` View-Space ışık yönü `bgfx::setUniform(u_lightDir, ...)` ile shader'a aktarılacak.

- `[x]` **3. Shader Güncellemesi (`fs_ssgi.sc`)**
  - `[x]` `uniform vec4 u_lightDir;` shader'a eklenecek.
  - `[x]` SSAO/SSGI döngüsü sonrası (veya öncesi) Contact Shadow (Ekran Uzayı Işın İzleme) fonksiyonu eklenecek.
  - `[x]` View-Space'te pikselden ışık yönüne doğru 8 adım raymarching yapılacak.
  - `[x]` Eğer çarpışma (occlusion) tespit edilirse gölge faktörü artırılacak.
  - `[x]` Elde edilen gölge faktörü ile `finalColor` karartılacak (`finalColor *= (1.0 - shadow)`).

- `[x]` **4. Derleme ve Test**
  - `[x]` `compile_shaders.bat` ile shader derlenecek.
  - `[x]` Motor CMake ile derlenip test edilecek.
