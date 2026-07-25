# Contact Shadows Walkthrough

Faz 7.3'ün son adımı olan **Contact Shadows (Temas Gölgeleri)** başarıyla eklendi! 🎉

## Yapılan Değişiklikler

### 1. Performans Odaklı Tasarım (SSGI Entegrasyonu)
Contact Shadows için tamamen yeni bir pass oluşturup fazladan bant genişliği (bandwidth) harcamak yerine, halihazırda Depth ve Normal haritalarını okuyan **SSGI Pass (`fs_ssgi.sc`)** içerisine bu özelliği entegre ettik.
- Böylece sadece ekstra birkaç matematik işlemiyle yüksek performanslı bir sonuç elde ettik.

### 2. View-Space Light Direction
- Kameranın dönüşüne göre ışık yönünün de View-Space (Kamera Uzayı) koordinatlarına çevrilmesi gerekiyordu.
- `Renderer.cpp` içerisinde kameranın `View Matrix`'inin sadece rotasyon kısmını kullanarak (3x3 çarpım) ışık yönünü `viewLightDir` adlı değişkene aktardık ve bunu shader'a gönderdik (`u_lightDir`).

### 3. Screen-Space Raymarching (Işın İzleme)
- `fs_ssgi.sc` içerisinde, her piksel için ışık yönüne doğru 8 adımlık bir ışın izleme döngüsü (Raymarch Loop) yazdık.
- Işın her adımda ekran koordinatına (UV) çevriliyor ve o noktadaki derinlik (Depth) okunuyor.
- Eğer okunan derinlik, ışının bulunduğu noktadan daha yakınsa (ve belirlenen kalınlık limitleri içerisindeyse) objenin ışığı kestiği anlaşılıyor (Occlusion).
- Bu durumda gölge faktörü devreye girip pikselin parlaklığını karartıyor. Ekranın uç kısımlarında yaşanabilecek yapaylıkları önlemek için gölge `smoothstep` kullanılarak kenarlara doğru hafifçe silinir hale getirildi (Edge fade).

## Sonuç
Oyun motoru başarıyla derlendi. Artık nesnelerin birbirine temas ettiği noktalarda gölge haritasının çözünürlüğünden bağımsız, piksel hassasiyetinde minik gölgeler göreceksiniz. Bu, Forward Rendering hattında **SSDO (Screen Space Directional Occlusion)** işlevi görerek nesnelerin yere çok daha sağlam basmasını sağlıyor.

---

Faz 7.3 tamamlandı! Sırada **Faz 7.4+** hedefleri var:
- **Görev 7: SSR (Screen Space Reflections)**
- **Görev 8: Temporal Anti-Aliasing (TAA) ve Temporal Gölge Filtreleme**

Öncelikle Görev 7 (SSR) ile planlamaya devam edebiliriz. Nasıl ilerlemek istersiniz?
