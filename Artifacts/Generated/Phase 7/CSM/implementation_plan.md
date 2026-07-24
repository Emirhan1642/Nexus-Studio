# Cascaded Shadow Maps (CSM) Uygulama Planı

Mevcut durumda motor, 2048x2048 boyutunda tek bir gölge haritası (Shadow Map) kullanmaktadır. Büyük sahnelerde veya kamera yakınlaştırmalarında bu tekil gölge haritası, piksellenmeye (Perspective Aliasing) yol açar. Bunu çözmek için kameranın görüş alanını (frustum) 3 parçaya bölüp her biri için ayrı gölge haritası (Cascade) oluşturacağız.

## Hedefler
1. Kamera görüş alanını (Frustum) derinliğe göre (Z ekseni) 3 parçaya bölmek (Cascade).
2. Her bir parça için ışığın bakış açısından projeksiyon (Ortho) matrisi hesaplamak.
3. Gölge haritası geçişlerinde pürüzsüz görünüm için piksellerin hangi cascade içerisinde olduğunu hesaplayan bir Shader yapısı kurmak.

## Önerilen Değişiklikler

---

### C++ (Motor) Değişiklikleri
#### [MODIFY] [Renderer.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Renderer.h)
- 3 farklı gölge `ViewId` (Örn: `View_ShadowCascade0`, `View_ShadowCascade1`, `View_ShadowCascade2`) tanımlanacak.
- Mevcut tekil `m_shadowMapFB` yerine 3 adet FrameBuffer (veya Texture2DArray) oluşturulacak. Uniform'lar için (örneğin 3 adet `u_lightMtx` ve Cascade mesafelerini tutacak `u_csmParams`) yeni bgfx UniformHandle nesneleri tanımlanacak.

#### [MODIFY] [Renderer.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Renderer.cpp)
- **Cascade Frustum Hesaplaması:** Kameranın görüş alanındaki Z değerlerini (Örn: `0-15m`, `15-50m`, `50-150m`) alarak bu noktalara denk gelen köşeleri (frustum corners) dünya koordinatlarına çevireceğiz.
- **Işık Projeksiyonu (Ortho Matrix):** Frustum köşelerini kapsayacak en uygun (Bounding Box) ışık projeksiyonunu (Orthographic) her üç cascade için hesaplayacağız.
- **Render Pass:** Mevcut döngü, her proxy objesini 3 ayrı pass üzerinden (farklı view port'lar veya TextureArray katmanları kullanarak) shadow shader'ına gönderecek şekilde genişletilecek.

---

### Shader Değişiklikleri
#### [MODIFY] [fs_pbr.sc](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Shaders/fs_pbr.sc)
- `u_csmParams` uniformu ile kameraya olan derinlik değerine (View-space Z veya doğrudan frag coord Z) bakarak uygun cascade seçimi yapılacak.
- Seçilen cascade'e ait `u_lightMtx` ile gölge uzayı hesaplanıp `s_texShadow` üzerinden PCF filtresi çalıştırılacak.
- Texture katmanları (Texture2DArray) ya da tek atlas üzerindeki UV ofsetleri yardımıyla üç farklı gölge çözünürlüğünden en uygunu kullanılacak.

#### [MODIFY] [varying.def.sc](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Shaders/varying.def.sc) ve [vs_pbr.sc](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Renderer/Shaders/vs_pbr.sc)
- Fragment shader'da derinliği (Z) tespit edebilmek için kameraya olan görünüm mesafesi (View Depth) veya dünya koordinatı üzerinden uzaklık eklenecek.
- 3 farklı `posLightSpace` aktarmak yerine, dönüşümü Fragment Shader tarafında yapmak (veya Vertex Shader'da dizi olarak paslamak) gerekecektir. Uniform sınırlarına takılmamak adına fragment shader'da `u_lightMtx[3]` üzerinden matris çarpımı yapılacaktır.

## Açık Sorular (Open Questions)

> [!IMPORTANT]
> - Cascade sayısı 3 olarak planlanmıştır. Mobil/Düşük donanım hedeflemiyorsak, standart kalitede pürüzsüzlük sağlayacaktır, kabul ediyor musunuz?
> - Performans tasarrufu açısından ayrı FrameBuffer yerine tek bir büyük Texture (Atlas, örn: 2048x6144) mi kullanalım, yoksa BGFX desteğine güvenip Texture2DArray (veya 3 farklı FrameBuffer üzerinden 3 ayrı sampler) mi yapalım? (En ideali ve BGFX ile sorun çıkarmayanı, geniş bir atlas kullanmaktır (örn: `4096x2048` atlas içinde yan yana haritalar). BGFX'in bazı arka uçlarında Texture2DArray kullanımı kısıtlı olabilir.)

## Doğrulama Planı (Verification)
- C++ derlemesinin yapılması.
- Editör içerisinde gölgelerin kenarlarının yakındayken çok daha net, uzaklaştıkça yavaşça çözünürlüğü düşürerek (Cascaded) piksellenmeden göründüğünün teyit edilmesi.
- Görsel doğrulama için her bir cascade geçişine debug rengi (Örn: Yakın=Kırmızı, Orta=Yeşil, Uzak=Mavi gölge) vererek test yapmak.
