# Task: Faz 7.1 LOD Sistemi (meshoptimizer)

- `[ ]` **Görev 1: Bağımlılıkların Eklenmesi (CMake)**
  - `[ ]` `CMakeLists.txt` dosyasına `FetchContent` ile `zeux/meshoptimizer` eklenecek.
  - `[ ]` `EngineAssets` (veya `EngineRenderer`) hedefine linklenecek.

- `[ ]` **Görev 2: Asset Import Sırasında LOD Üretimi**
  - `[ ]` `AssetImportPipeline` (veya `SkeletalMeshImporter`) içerisinde `meshopt_simplify` fonksiyonu çağrılarak LOD zinciri oluşturulacak.
  - `[ ]` Orijinal meshin (LOD0) yanı sıra, `LOD1` (%50) ve `LOD2` (%15) indeks dizileri üretilecek.
  - `[ ]` `ImportedSkeletalMesh` yapısına birden fazla `std::vector<uint32_t> indices` veya struct listesi eklenecek.

- `[ ]` **Görev 3: RendererSystem Güncellemesi**
  - `[ ]` `Renderer.h` içerisindeki `MeshData` yapısına `bgfx::IndexBufferHandle ibhLods[3]` eklenecek (Veya dinamik vektör).
  - `[ ]` `RendererSystem::getMeshHandle` gibi mesh kayıt/yükleme fonksiyonları güncellenerek LOD'ların GPU'ya yüklenmesi sağlanacak.

- `[ ]` **Görev 4: Runtime'da Mesafe Bazlı LOD Seçimi**
  - `[ ]` `RendererSystem::renderFrame` döngüsünde, her `MeshComponent` çizilirken kameraya olan mesafe (distance) hesaplanacak.
  - `[ ]` Mesafeye göre uygun olan LOD indeks buffer'ı `bgfx::setIndexBuffer` ile GPU'ya gönderilecek.

- `[ ]` **Görev 5: Derleme ve Test**
  - `[ ]` Proje derlenecek.
  - `[ ]` Kamerayı modelden uzaklaştırıldığında performansın arttığı ve poligon sayısının düştüğü doğrulanacak.
