# Faz 7.3 - Görev 6: Contact Shadows Uygulama Planı

Bu görevde, Screen-Space (Ekran Uzayı) derinlik verisini kullanarak objelerin birbiriyle temas ettiği noktalarda (örneğin ayakların yere basması, ufak taşlar vb.) oluşan küçük detay gölgelerini belirginleştiren **Contact Shadows** özelliği eklenecektir.

## 1. Mimari Karar
Mevcut motor mimarisi **Forward Rendering** (İleri Yönlü Render) tabanlı çalıştığı için ana aydınlatma ve gölgelendirme (CSM) tek bir pass içerisinde yapılmaktadır. Contact Shadows işlemini ayrı bir tam ekran (full-screen) pass yapmak yerine, zaten ekran uzayında SSAO ve SSGI hesaplayan ve Depth/Normal haritalarına erişimi olan **SSGI Pass (`fs_ssgi.sc`)** içerisine entegre edeceğiz. Bu sayede bellek bant genişliğinden (bandwidth) tasarruf edilecek ve performans artacaktır.

## 2. C++ (Renderer.cpp) Değişiklikleri
- Işık yönü (Light Direction) şu an `Renderer.cpp` içerisinde hardcoded (`{0.577f, 0.577f, -0.577f}`) olarak durmaktadır. Bu değeri SSGI shader'ına aktarabilmek için `u_lightDir` adında yeni bir uniform (Vec4) oluşturulacak.
- Işık yönü, kameranın View Matrisi ile çarpılarak **View-Space (Kamera Uzayı)** koordinatlarına çevrilecek ve shader'a bu şekilde yollanacaktır. Çünkü SSGI shader'ı hesaplamalarını View-Space'te yapmaktadır.

## 3. Shader Değişiklikleri (`fs_ssgi.sc`)
- `u_lightDir` uniform'u eklenecektir.
- SSAO/SSGI döngüsüne ek olarak, bir **Raymarching (Işın İzleme)** döngüsü eklenecektir.
- **İşlem Adımları:**
  1. Pikselin kamera uzayındaki pozisyonundan başlanarak ışık yönüne doğru kısa bir mesafe (Örn: 8 adım) ilerlenir.
  2. Her adımda bulunulan nokta projeksiyon matrisiyle (Projection Matrix) ekran uzayına (UV) çevrilir.
  3. O noktadaki derinlik haritası (Depth Buffer) okunur ve mevcut noktanın derinliği ile karşılaştırılır.
  4. Eğer derinlik haritasındaki obje, ışının önündeyse bir engelleme (occlusion) var demektir ve Contact Shadow uygulanır.
- Elde edilen gölge faktörü, pikselin mevcut rengini (Ambient harici) karartmak için kullanılacaktır.

---

> [!IMPORTANT]
> **Kullanıcı İncelemesi / Açık Sorular:**
> 1. Contact Shadows, Forward Rendering mimarimizde aydınlatma sonrası (Post-Process) bir etki olarak çalışacağı için hem direkt ışığı hem de ortam ışığını bir miktar karartacaktır (Screen Space Directional Occlusion - SSDO gibi davranacaktır). Bu, gerçekçilik açısından AAA oyunlarda da kullanılan pratik ve kabul edilebilir bir çözümdür. Onaylıyor musunuz?
> 2. Performans için ışın izleme adımı (Raymarch Steps) sayısını 8 veya 16 olarak sınırlandırmayı planlıyorum. Geçerli midir?
