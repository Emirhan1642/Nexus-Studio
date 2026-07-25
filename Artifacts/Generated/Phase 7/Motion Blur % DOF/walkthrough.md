# Motion Blur ve Depth of Field (DOF) Walkthrough

Post-Processing zincirine sinematik bir his katan iki önemli özellik başarıyla eklendi: **Depth of Field (Alan Derinliği)** ve **Camera Motion Blur (Kamera Hareket Bulanıklığı)**.

## Yapılan Değişiklikler

### 1. Render Pipeline Genişletildi
- `Renderer.h` ve `Renderer.cpp` içerisinde Tonemapping (SDR dönüşümü) adımından hemen önce çalışacak şekilde 2 yeni **HDR FrameBuffer** (`m_dofFB` ve `m_mbFB`) oluşturuldu.
- Böylece efekt zinciri şu şekilde güncellendi: 
  `SSGI -> Bloom -> DOF -> Motion Blur -> Tonemap`
- Bu sayede alan derinliği ve hareket bulanıklığı doğrusal renk uzayında (Linear HDR) uygulandığı için çok daha doğal sonuçlar vermektedir.

### 2. Depth of Field (DOF) Shader'ı
- `fs_dof.sc` adında yeni bir shader yazıldı.
- G-Buffer'daki **Depth Buffer** okundu ve kameranın projeksiyon değerlerine göre doğrusal bir derinlik (Linear Depth) değerine dönüştürüldü.
- **Focus Distance (Netleme Mesafesi)** ve **Focal Range (Netleme Aralığı)** kullanılarak piksellerin bulanıklık çapı (Circle of Confusion) hesaplandı.
- Poisson-Disk dağılımı kullanılarak, bulanık piksellere 8 örneklemli bir Gauss-vari blur uygulandı.

### 3. Motion Blur Shader'ı
- `fs_mb.sc` adında yeni bir shader yazıldı.
- C++ tarafında bir önceki karenin (frame) View-Projection matrisi (`u_prevViewProj`) saklanıp shader'a aktarıldı.
- Shader, G-Buffer'daki derinlik bilgisini alıp pikselin Dünya Koordinatı'nı (World Position) buluyor.
- Daha sonra bu dünya koordinatı, kameranın bir önceki karedeki yerine göre (Projection) çarpılarak pikselin bir önceki ekrandaki konumu (Previous NDC) hesaplanıyor.
- Şimdiki ve önceki ekran koordinatları arasındaki fark (Velocity Vector) hız vektörü olarak kullanılıyor.
- Renkler bu hız vektörü çizgisi boyunca çoklu örneklemeyle birleştirilerek hareket bulanıklığı sağlanıyor.

### 4. Optimizasyonlar
- Hem DOF hem de Motion Blur shader'ları ağır döngüler (16-32 sample) yerine, MVP aşaması için optimize edilmiş **8 sample** kullanılarak yazıldı.
- Motion Blur'da kamera çok hızlı döndüğünde görüntünün aşırı bozulmasını önlemek için blur boyutu `length(velocity) > 0.05` şartıyla sınırlandırıldı (Clamping).

## Sonuç
Oyun motoru başarıyla derlendi. Artık kamerayı hızlıca çevirdiğinizde veya hareket ettirdiğinizde nesneler bulanıklaşacak ve belli bir mesafenin ötesindeki objeler alan derinliği etkisiyle (Bokeh/Blur) arka planda kaybolacaktır.
