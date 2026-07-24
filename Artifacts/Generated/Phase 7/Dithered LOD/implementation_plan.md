# Dithered LOD Transitions Uygulama Planı

Bu aşamada, kameraya olan mesafeye göre değişen LOD (Level of Detail) modellerinin geçişi sırasında oluşan ani belirmeleri/yok olmaları (pop-in efekti) önlemek için **Mesafe Tabanlı Dither (Erime) Geçişi** uygulanacaktır.

## Önerilen Değişiklikler

### 1. C++ Tarafı: Mesafe ve Geçiş Hesaplaması
LOD geçişleri `Renderer.cpp` içerisinde mesafe (`distSq`) bazlı yapılmaktadır.
Mevcut durumda:
- Mesafe < 50m: LOD 0
- 50m < Mesafe < 100m: LOD 1
- Mesafe > 100m: LOD 2

**Yeni Sistemde:**
Belirli geçiş bölgeleri (Transition Zones) tanımlanacaktır. Örneğin 45m ile 55m arasında bir obje hem LOD 0 hem de LOD 1 olarak **iki kez** çizilecektir.
- **LOD 0** için dither/fade değeri 1.0'dan 0.0'a doğru azalacak (eriyip kaybolacak).
- **LOD 1** için dither/fade değeri 0.0'dan 1.0'a doğru artacak (belirecek).

Bunun için:
- `Renderer.h` içerisine `bgfx::UniformHandle u_lodParams` eklenecek.
- `Renderer.cpp` `init` kısmında `u_lodParams = bgfx::createUniform("u_lodParams", bgfx::UniformType::Vec4)` oluşturulacak.
- `Renderer.cpp` render döngüsünde (line 510 civarı) mesafe kontrolü güncellenip, geçiş bölgesinde olan objeler iki ayrı draw call (LOD A ve LOD B) ile submit edilecek.

### 2. Shader Tarafı (`fs_pbr.sc` ve `fs_shadow.sc`)
- PBR ve Gölge Fragment Shader'larına `uniform vec4 u_lodParams;` eklenecek. `x` bileşeni `fade` (0.0 ile 1.0 arası) değerini tutacak.
- Ekran koordinatları (`gl_FragCoord.xy`) kullanılarak 4x4 veya 8x8'lik bir **Bayer Matrisi (Dither Pattern)** hesaplanacak.
- Eğer `u_lodParams.x < DitherThreshold` ise `discard;` komutu ile piksel çizilmeyecek. (Bu yöntem şeffaflık kullanmadığı için Depth Buffer'ı bozmaz ve performanslıdır).

---

> [!IMPORTANT]
> **Açık Soru (Onay Bekliyor):**
> 1. Geçiş mesafesini şimdilik **±5 birim** (Örn: 45m - 55m arası) olarak sabit tanımlamayı öneriyorum. Uygun mudur?
> 2. Gölgelerin de (Shadow Maps) geçiş sırasında dither olmasını istiyor musunuz? Yoksa gölgeler performans açısından anında mı değişsin? (Genelde gölgelerin de dither olması daha pürüzsüz görünür ancak shadow pass shader'ını güncellemeyi gerektirir. Planda PBR ve Gölge shaderlarını beraber güncellemeyi öngördüm.)
