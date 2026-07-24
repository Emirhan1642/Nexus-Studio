# Faz 7.2: Voxel Cone Tracing (VCT) Implementasyon Planı

Bu plan, **Faz 7.2'nin Görev 1'i** olan **Voxel Cone Tracing (VCT) tabanlı Global Illumination (GI)** özelliğinin `Nexus Studio` motoruna entegrasyonunu kapsamaktadır.

## 1. Hedef
Oyun içindeki kapalı ve gölgeli alanların, çevreden seken ışıklar (dolaylı aydınlatma) ile gerçekçi bir şekilde aydınlatılmasını sağlamak. VCT yaklaşımı, sahnelerin çalışma anında (dinamik) olarak hesaplanmasına izin verir. Bu sayede objeler hareket ettiğinde ışıklandırma da anında güncellenir.

## 2. Tasarım & Mimari Kararları

### 2.1. 3D Voxel Grid ve Voxelizer Sınıfı
Sahne, 3D bir dokuya (Texture3D) çevrilecek (Örn. 256x256x256 boyutlarında).
- **[NEW]** `Engine/Renderer/GI/Voxelizer.h/cpp`: `bgfx::createTexture3D` ile bir Compute-Writable (UAV) doku oluşturulacak. Sahneyi bu dokuya kaydetmek için ayrı bir Render Pass (`View_Voxelize`) tanımlanacak.
- **[MODIFY]** `Engine/Renderer/Renderer.h/cpp`: Voxelizer'ın başlatılması ve sahne render döngüsü içerisinden çağrılması eklenecek.

### 2.2. Voxelization Shader'ları
Mesh'leri voxel'lere çeviren özel shader'lar.
- **[NEW]** `Engine/Renderer/Shaders/vs_voxelize.sc` ve `fs_voxelize.sc`: Objenin pozisyonuna göre 3D texture içerisindeki koordinatı hesaplayıp, `imageStoreAtomic` kullanarak Voxel Grid içerisine Albedo/Normal/Emissive renkleri yazacak (Compute Write özelliğini kullanarak fragment shader üzerinden UAV erişimi).
- **[NEW]** `Engine/Renderer/Shaders/cs_voxel_mipmap.sc`: Voxel 3D dokusu için Mipmap seviyelerini Compute Shader ile üretecek. Cone tracing işlemi uzak mesafeler için bu mipmap'leri okur.

### 2.3. Cone Tracing ile GI Hesabı
- **[MODIFY]** `Engine/Renderer/Shaders/fs_pbr_forward.sc` (veya `fs_main.sc`): PBR hesabına (Cook-Torrance) ek olarak Voxel 3D Texture'ı eklenecek (`s_texVoxel`). Yüzey normali etrafında koniler fırlatılarak `traceCone` fonksiyonu ile ortam ışığı toplanıp ana renge eklenecek.

---

> [!IMPORTANT]
> **User Review Required:**
> 1. Voxel çözünürlüğünü (Voxel Grid Size) başlangıç için **256x256x256** olarak belirledik. (Performans ve kalite için iyi bir orta noktadır). Onaylıyor musunuz?
> 2. Voxelization için **Fragment Shader + UAV Atomic Write** tekniğini kullanacağız. bgfx'in Compute/UAV özelliklerinden tam yararlanacağız. Plan uygun mudur?

## 3. Doğrulama (Verification)
1. Yeni `vs/fs_voxelize`, `cs_voxel_mipmap` shader'ları `compile_shaders.bat`'a eklenip derlenecek.
2. Editörde bir oda (duvarlı kapalı alan) kurulacak, ışık dışarıdan verilecek.
3. İçeri giren ışığın duvarlardan sekerek gölgeli alanları aydınlattığı (Global Illumination efekti) görsel olarak test edilecek.

Lütfen bu planı onaylayın, ardından shader kodlamasıyla başlayalım.
