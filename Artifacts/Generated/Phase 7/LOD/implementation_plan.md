# Faz 7.1 - Dinamik Çözünürlük ve LOD Sistemi (Performans)

SSGI/SSAO ile görsel kaliteyi oldukça artırdık, fakat performans maliyeti de arttı. Bunun önüne geçmek için **LOD (Level of Detail)** sistemini devreye sokuyoruz. Uzaktaki objeleri çok daha az poligonla (üçgenle) çizerek GPU'nun üzerindeki yükü ciddi oranda hafifleteceğiz.

## Mimari Yaklaşım

Nanite öncesi endüstri standardı olan "Discrete LOD" (Ayrık Detay Seviyeleri) sistemini kullanacağız. Bu sistemde CPU, objenin kameraya olan mesafesine veya ekranda kapladığı alana bakarak uygun detay seviyesini (LOD0, LOD1, LOD2 vb.) seçer.

Mesh basitleştirme (decimation) algoritmasını sıfırdan yazmak yerine, sektör standardı olan ve birçok AAA motorda da kullanılan açık kaynaklı **meshoptimizer** kütüphanesini CMake ile projemize entegre edeceğiz.

## Önerilen Değişiklikler

### 1. `meshoptimizer` Entegrasyonu
- `CMakeLists.txt` dosyasına `FetchContent` ile [meshoptimizer](https://github.com/zeux/meshoptimizer) kütüphanesi eklenecek.

### 2. Asset Pipeline (LOD Üretimi)
- `AssetImportPipeline` güncellenecek. `SkeletalMeshImporter` (veya statik mesh importu) çalıştıktan sonra, `meshoptimizer` kullanılarak orijinal mesh'in (LOD0) daha düşük poligonlu versiyonları (Örn: LOD1 = %50 üçgen, LOD2 = %15 üçgen, LOD3 = %5 üçgen) üretilecek.
- Güzelliği şu ki, basitleştirme sadece **Index Buffer (İndeks belleği)** üzerinde yapılır; orijinal Vertex Buffer aynı kalır. Bu sayede bellekten büyük oranda tasarruf ederiz.
- `ImportedSkeletalMesh` yapısı güncellenerek, bir dizi (array) indeks dizisi tutması sağlanacak.

### 3. RendererSystem (LOD Seçimi)
- `Renderer.h` içerisindeki `MeshData` yapısı güncellenerek birden fazla `bgfx::IndexBufferHandle` tutması sağlanacak (Her LOD için bir tane).
- `renderFrame` metodunda, her obje çizilmeden önce kameraya olan mesafesi (veya ekranda kapladığı tahmini boyut) hesaplanacak.
- Mesafeye göre uygun LOD (Index Buffer) seçilerek `bgfx::setIndexBuffer` ile gönderilecek.

---

> [!IMPORTANT]
> **User Review Required:**
> Bu MVP (Başlangıç) aşamasında, objenin LOD seviyesini basit bir "Mesafeye Göre" (Distance-based) mi seçelim, yoksa "Ekranda Kapladığı Yüzdeye Göre" (Screen-size / Bounding Sphere) mi hesaplayalım?
> 
> *Önerim*: Şimdilik hızlı test edebilmek için "Mesafeye Göre" ilerlemek, daha sonra (Bounding Box/Sphere yapımız geliştikçe) Screen-Size'a geçmektir. Bu yaklaşımı onaylıyor musunuz?

---

## Doğrulama Planı
1. `meshoptimizer` kütüphanesi başarıyla derlenip projeye bağlanacak.
2. Bir obje (tercihen yüksek poligonlu) içeri aktarılacak.
3. Editörde kamerayı objeden uzaklaştırdıkça poligon sayısının anında düştüğü (wireframe modunda veya gözle) gözlemlenecek.

Onayladığınız takdirde `CMakeLists.txt` üzerinden başlayarak uygulamaya geçeceğim.
