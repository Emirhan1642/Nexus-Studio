# Faz 7.4 - Görev 8: TAA (Temporal Anti-Aliasing) ve Temporal Filtreleme Uygulama Planı

TAA (Geçici Kenar Yumuşatma), önceki karelerin verilerini kullanarak hem kenarlardaki kırılmaları (aliasing) hem de SSR/SSGI/Gölgeler gibi efektlerdeki titremeleri (flickering) büyük ölçüde azaltan modern bir post-process tekniğidir.

## 1. Mimari Tasarım
TAA, hareket vektörlerini (Motion Vectors) kullanarak bir önceki karenin (History) piksellerini mevcut kareye yansıtır (Reprojection). Ghosting (hayalet izi) sorununu çözmek için mevcut karenin 3x3 komşuluk piksellerine bakılarak History rengi kırpılır (Neighborhood Clamping/Clipping).

Efekt Sıralaması (Yeni):
`Main Pass` -> `SSGI` -> `SSR` -> `DOF` -> `Motion Blur` -> **`TAA Pass`** -> `Tonemap` -> (`FXAA` isteğe bağlı)
*Not: TAA, HDR uzayında yapılmalıdır. Bu yüzden Tonemap'ten hemen önce çalıştırılacak.*

## 2. C++ Tarafı (`Renderer.h` & `Renderer.cpp`)
- **Ping-Pong Buffer:** TAA için 2 adet FrameBuffer (`m_taaFB[0]` ve `m_taaFB[1]`) eklenecek. Her karede biri okuma (History) diğeri yazma (Current) için kullanılacak ve yer değiştirecekler.
- **Jittering (Titretme):** Kameranın Projeksiyon Matrisine her karede alt-piksel (sub-pixel) seviyesinde rastgele (Halton Sequence) bir kaydırma (Jitter) eklenecek. Bu, kenarların zamanla yumuşamasını sağlar.
- **Render View:** `View_TAA = 35` eklenecek.
- **Uniformlar:** `u_taaParams` eklenecek. (x: Jitter X, y: Jitter Y, z: Blend Factor, w: Geçmiş Çözünürlük Skalası vb.)
- Hâlihazırda bulunan `u_invViewProj` ve `u_prevViewProj` matrisleri kullanılarak TAA shader'ı içinde Hız (Velocity) hesaplanacak.

## 3. TAA Shader'ı (`fs_taa.sc`)
- **Girdiler:** 
  - `s_texColor` (Current Frame - Motion Blur çıktısı)
  - `s_texHistory` (Önceki TAA çıktısı)
  - `s_texDepth` (Hız hesaplaması için)
- **Mantık:**
  1. Depth buffer'dan dünya pozisyonu, ardından önceki karenin ekran pozisyonu (Velocity) hesaplanır.
  2. `uv - velocity` kordinatından `s_texHistory` okunur.
  3. `s_texColor` üzerinden 3x3 piksellik bir alanda Min ve Max renk değerleri bulunur.
  4. Okunan History rengi, bu Min ve Max değerleri arasına sıkıştırılır (Clamping). Bu, Ghosting'i büyük oranda çözer.
  5. Mevcut renk ile History rengi harmanlanır (Örn: %10 Current, %90 History).

## 4. Jitter Yönetimi
- Projeksiyon matrisine eklenen Jitter, diğer post-process efektlerinin (SSR, DOF vb.) pozisyon hesaplamalarını bozabilir. Bunu engellemek için post-processlere verilen `u_invProj` matrisi **Jitter içermeyen** orijinal matris olmalıdır. 

---

> [!IMPORTANT]
> **Kullanıcı İncelemesi / Açık Sorular:**
> 1. Jittering (Kamera Projeksiyon titretmesi) tüm sahnede keskinliği artırıp kenarları pürüzsüzleştirir ancak entegrasyonu hassastır. Jitter ekleyerek tam bir TAA mı yapalım, yoksa şimdilik projeksiyonu değiştirmeden sadece Temporal Denoising (Titreme azaltıcı Reprojection) mi yapalım? (Önerim tam TAA yapılması).
> 2. TAA entegre edildiğinde FXAA pasif duruma getirilebilir veya ikisi birden açık bırakılabilir. Genelde TAA yeterli olur. FXAA'yı kapatmamı veya opsiyonel yapmamı ister misiniz?
