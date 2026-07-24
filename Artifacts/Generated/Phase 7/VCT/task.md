# Voxel Cone Tracing (VCT) İş Listesi

- `[x]` **1. Veri Modeli ve Sınıf**
  - `[x]` `Engine/Renderer/GI` klasörünün oluşturulması.
  - `[x]` `Voxelizer.h/cpp` dosyalarının yazılması (3D Texture, Pass ayarları).
  - `[x]` `RendererSystem` içerisine entegrasyon.

- `[x]` **2. Voxelization Shader'ları**
  - `[x]` `vs_voxelize.sc`: Ortografik projeksiyon (gerekirse geometry shader veya dominant axis seçimi).
  - `[x]` `fs_voxelize.sc`: `imageStoreAtomic` ile Texture3D yazımı.
  - `[ ]` `cs_voxel_mipmap.sc`: (Opsiyonel / İleri Seviye) 3D Mipmap üretimi.
  - `[x]` `compile_shaders.bat` dosyasının güncellenmesi.

- `[x]` **3. Cone Tracing (Ana Shader)**
  - `[x]` PBR fragment shader'ına (`fs_main.sc` vs.) `s_texVoxel` eklenmesi.
  - `[x]` `traceCone` fonksiyonunun yazılması ve dolaylı aydınlatmanın ana renge eklenmesi.

- `[x]` **4. Test ve Doğrulama**
  - `[x]` Projenin derlenmesi.
  - `[x]` Editörde GI efektinin doğrulanması (Hazırlandı, kullanıcı tarafından test edilecek).
