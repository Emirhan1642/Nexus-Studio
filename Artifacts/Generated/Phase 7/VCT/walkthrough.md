# Phase 7.2 - Voxel Cone Tracing (VCT) Uygulaması Özeti

Global Illumination (GI) için **Voxel Cone Tracing** altyapısını başarıyla tamamladık ve Nexus Studio motoruna entegre ettik.

## Neler Yapıldı?

### 1. Voxelizer Altyapısı (`Voxelizer.h` ve `Voxelizer.cpp`)
- `Engine/Renderer/GI` modülü oluşturuldu.
- `bgfx::Texture3D` kullanılarak `256x256x256` çözünürlüğünde, okuma/yazma (UAV) destekli bir Voxel grid tanımlandı.
- `RendererSystem` içerisine özel bir "Voxelization Pass" eklendi. Bu pass, ana gölgelendirmeden (main color pass) önce tüm sahnedeki (Bounding Box) modelleri voxel grid içerisine rasterize eder.

### 2. Voxelization Shader'ları
- **`vs_voxelize.sc`**: Klasik model-görünüm-projeksiyon dönüşümü yerine sadece dünya koordinatını aktarır ve Geometry Shader eksikliğini gidermek amacıyla doğrudan `v_position` üzerinden işlem yapar (Dominant axis seçimi olmadan, doğrudan X-Z projeksiyonu tabanlı temel yaklaşım kullanılmıştır).
- **`fs_voxelize.sc`**: BGFX'in desteklediği atomik yazma işlemi (`imageAtomicMax` ve ilgili makrolar) kullanılarak `u1` slotunda (5 no'lu register slotunda) 3D Texture'a (UAV) her bir pikselin dünya koordinatı üzerinden voxel pozisyonu yazılmıştır. Voxel yoğunluğu (alpha/luminance değeri) atomik olarak biriktirilir.
- `compile_shaders.bat` güncellendi ve Voxelization shader'ları başarılı şekilde HLSL'e (s_5_0 / D3D11) derlendi.

### 3. Ana PBR Shader Modifikasyonu (`fs_pbr.sc`)
- Voxel verisini okumak için `SAMPLER3D(s_texVoxel, 5)` eklendi.
- `traceCone` fonksiyonu yazılarak fragment'in (piksel) dünya konumundan normal vektörüne doğru bir koni izlemesi (Cone Tracing) sağlandı.
- Mipmap (CS - Compute Shader ile downsample) yapmadığımız için LOD(0) üzerinden sabit adımlarla alpha-blending yapılarak birikimli dolaylı ışık (Indirect GI) hesaplandı.
- Elde edilen dolaylı difüz renk, Ambient ışığıyla birleştirilerek PBR formülüne entegre edildi.

### 4. Entegrasyon ve Derleme
- `Renderer.cpp` içerisinde oluşan imza ve framebuffer resize çakışmaları düzeltilip eski Post-Processing ve Bloom zincirlerinin çalışması sağlandı.
- Proje (CMake/Ninja veya MSBuild) sorunsuz bir şekilde derlendi ve "Build Succeeded" alındı.

## Sıradaki Adımlar (Sonraki Görevler İçin)
Bu özellik test edilmek üzere editöre eklenmiştir. Aşağıdaki kısımlar bir sonraki fazlarda ele alınabilir:
- **Compute Shader Mipmapping**: VCT koni izleme performansı ve pürüzlülük için 3D Texture mipmap'lerinin GPU tarafında hiyerarşik olarak oluşturulması.
- **Cascaded Shadow Maps (CSM)**: Yakın, orta ve uzak mesafe için gölge kalitesini artıracak kaskad gölge altyapısının eklenmesi.

## Doğrulama Nasıl Yapılır?
Nexus Studio uygulamasını açtıktan sonra sahneye bir kapalı oda veya kutular yerleştirin, ve doğrudan ışık almayan köşelerin aydınlandığını (GI Color Bleeding) gözlemleyebilirsiniz.
