# Task: Faz 7.1 LOD Sistemi (meshoptimizer)

- `[x]` **Görev 1: Bağımlılıkların Eklenmesi (CMake)**
  - `[x]` `CMakeLists.txt` dosyasına `FetchContent` ile `zeux/meshoptimizer` eklenecek.
  - `[x]` `EngineAssets` (veya `EngineRenderer`) hedefine linklenecek.

- `[x]` **Görev 2: Asset Import Sırasında LOD Üretimi**
  - `[x]` `AssetImportPipeline` (veya `SkeletalMeshImporter`) içerisinde `meshopt_simplify` fonksiyonu çağrılarak LOD zinciri oluşturulacak.
  - `[x]` Orijinal meshin (LOD0) yanı sıra, `LOD1` (%50) ve `LOD2` (%15) indeks dizileri üretilecek.
  - `[x]` `ImportedSkeletalMesh` yapısına birden fazla `std::vector<uint32_t> indices` veya struct listesi eklenecek.

- `[x]` **Görev 3: RendererSystem Güncellemesi**
  - `[x]` `Renderer.h` içerisindeki `MeshData` yapısına `bgfx::IndexBufferHandle ibhLods[3]` eklenecek (Veya dinamik vektör).
  - `[x]` `RendererSystem::getMeshHandle` gibi mesh kayıt/yükleme fonksiyonları güncellenerek LOD'ların GPU'ya yüklenmesi sağlanacak.

- `[x]` **Görev 4: Runtime'da Mesafe Bazlı LOD Seçimi**
  - `[x]` `RendererSystem::renderFrame` döngüsünde, her `MeshComponent` çizilirken kameraya olan mesafe (distance) hesaplanacak.
  - `[x]` Mesafeye göre uygun olan LOD indeks buffer'ı `bgfx::setIndexBuffer` ile GPU'ya gönderilecek.

- `[x]` **Görev 5: Derleme ve Test**
  - `[x]` Proje derlenecek.
  - `[x]` Kamerayı modelden uzaklaştırıldığında performansın arttığı ve poligon sayısının düştüğü doğrulanacak.
