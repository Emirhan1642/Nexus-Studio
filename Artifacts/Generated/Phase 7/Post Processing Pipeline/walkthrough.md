# Faz 7.1 (MVP) Post-Processing Tamamlandı!

Gelişmiş Grafik aşamasının (Faz 7) en büyük ve en kritik temeli olan **Post-Processing (Son İşlem)** mimarisi başarıyla motora eklendi.

## Neler Değişti?

### 1. HDR (High Dynamic Range) Çizim Hattı
Eskiden motor doğrudan ekrana (Backbuffer) çizim yapıyordu ve parlaklık değerleri 0 ile 1 arasına sıkışıyordu. Şimdi sahne 16-bit Floating Point bir ara Framebuffer'a (`RGBA16F`) çiziliyor. Bu sayede güneş ışığı veya parlak yüzeyler gerçek dünyadaki gibi sonsuz parlaklık seviyelerine çıkabiliyor.

### 2. Bloom (Parlama) Efekti ve Ping-Pong Mimarisi
Ekranda en parlak (Threshold üzerindeki) kısımları alıp, çözünürlüğü giderek küçülen 5 farklı Framebuffer'dan (Downsample & Ping-Pong Blur) geçirdik.
- **`fs_bloom_threshold.sc`**: Parlak alanları seçer.
- **`fs_bloom_blur.sc`**: Ayrıştırılmış alanları Gauss bulanıklaştırmasıyla yumuşatır.

### 3. ACES Filmic Tonemapping
HDR görüntüyü ve Bloom görüntüsünü birleştirip monitörlerin gösterebileceği renk uzayına (SDR) güvenli ve sinematik bir şekilde çeviren **ACES Filmic Tonemap** shader'ı (`fs_tonemap.sc`) eklendi. Bu sayede aşırı ışık altında renkler "patlayıp bembeyaz olmak" yerine, yavaşça ve doğal bir şekilde sarı/turuncuya kayarak beyazlaşır (tıpkı sinema kameralarında olduğu gibi).

## Doğrulama / Test Aşaması
Şu an `NexusStudioEditor.exe`'yi açtığınızda sahnede post-processing aktif olacak. Tam etkisini görmek için:
1. `Main.cpp` üzerinden veya Editörün içindeki menüden sahneye aşırı parlak bir küp (`emissive = 5.0` gibi) ekleyin.
2. Bu küpün etrafında gerçekçi bir sinematik ışık saçılması (Bloom) olduğunu göreceksiniz.

Sıradaki hedef olarak SSGI mı yoksa LOD sistemini mi istersiniz? Veya testlerinizi bitirdikten sonra sıradaki hedefe birlikte karar verebiliriz!
