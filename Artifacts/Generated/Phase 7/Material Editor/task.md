# Task: Faz 7.1 Node-Based Materyal Editörü

- `[x]` **Görev 1: Kütüphane Entegrasyonu (CMake)**
  - `[x]` `ThirdParty/CMakeLists.txt` dosyasına `FetchContent` ile `Nelarius/imnodes` kütüphanesi eklenecek.
  - `[x]` `imnodes`, `Editor` modülüne (veya ImGuiLayer'a) linklenecek.

- `[x]` **Görev 2: Veri Modeli ve Temel UI (ShaderGraph)**
  - `[x]` `Engine/Renderer/Materials/ShaderGraph.h` oluşturulacak (`ShaderNode`, `ShaderLink` yapıları).
  - `[x]` `Editor/Panels/MaterialEditorPanel.h/cpp` oluşturulacak (imnodes context yönetimi).
  - `[x]` Arayüzde sağ tık menüsü ile "Color", "TextureSample", "Multiply" gibi düğümler eklenebilmeli. (Örn: Albedo Color, Multiply).

- `[x]` **Görev 3: ShaderGraph Derleyicisi (Compiler)**
  - `[x]` `Engine/Renderer/Materials/ShaderGraphCompiler.h/cpp` oluşturulacak.
  - `[x]` Düğümleri okuyup Bgfx `.sc` formatında (fragment shader) C++ string olarak metin üreten kod yazılacak.
  - `[x]` Topolojik sıralama algoritması ile hesaplama sırası çözülecek.

- `[x]` **Görev 4: Runtime Shaderc Entegrasyonu**
  - `[x]` Üretilen string geçici bir dosyaya kaydedilip, `shaderc.exe` komut satırından `system()` veya `CreateProcess` benzeri bir yöntemle çağrılacak.
  - `[x]` Derlenen `.bin` dosyası belleğe okunup `bgfx::createProgram` ile motorda aktif edilecek.

- `[x]` **Görev 5: Test ve Doğrulama**
  - `[x]` Proje derlenecek.
  - `[x]` Yeni editörde bir renk düğümü "Output" düğümüne bağlanıp "Compile" butonuna basıldığında sahnedeki objenin renginin anında değiştiği doğrulanacak.
