# Faz 7.1 (MVP) - SSGI (Screen Space Global Illumination) & SSAO

Post-Processing altyapımızı kurduğumuza göre artık ekran uzayı ışıklandırma efektlerine geçebiliriz. Bu plan, motorumuza **SSGI** (Ekran Uzayı Dolaylı Aydınlatma) ve **SSAO** (Ekran Uzayı Ortam Gölgelemesi) eklemeyi hedefler.

## Mimari Zorluk ve Çözüm (MRT - Multiple Render Targets)
Şu anda motorumuz **Forward Renderer** (İleri Yönlü Oluşturucu) mantığıyla çalışıyor ve sadece "Renk" ve "Derinlik" çiziyor. SSGI ve SSAO'nun çalışabilmesi için ekranda gördüğümüz her pikselin **Normal (Yüzey Yönü)** bilgisine ihtiyacımız var. 

Bunu çözmek için Ana Çizim (Main Pass) aşamasını **MRT (Çoklu Çizim Hedefi)** yapısına geçireceğiz:
- `gl_FragData[0]`: HDR Renk
- `gl_FragData[1]`: Ekran Uzayı Normalleri (Screen-Space Normals)

## Önerilen Değişiklikler

### 1. C++ Altyapısı (Renderer.cpp)
- `m_hdrFB` (HDR Framebuffer) güncellenerek içine 1 adet daha Texture eklenecek: `Normal Buffer` (Örn. `RGBA8` formatında).
- Yeni SSGI/SSAO Shader Programları (`m_ssgiProgram`) yüklenecek.
- Post-processing zincirine (Bloom'dan hemen önce) SSGI geçişi eklenecek. Bu geçiş Renk, Derinlik ve Normal buffer'larını okuyup mevcut HDR renge dolaylı aydınlatmayı katacak.

### 2. PBR Shader Güncellemesi (fs_pbr.sc)
- `gl_FragColor` yerine MRT kullanımına geçilecek.
- Görüntünün HDR rengi `gl_FragData[0]`'a yazılırken, `v_normal` bilgisi kodlanarak `gl_FragData[1]`'e yazılacak.

### 3. Yeni GPU Shader'ları
- **`fs_ssgi.sc`**: Ekrana tam oturan bir quad üzerinde çalışacak. Kendi etrafındaki piksellerin derinlik ve normallerine bakarak (Ray-marching) onlardan seken renkleri (dolaylı aydınlatmayı) hesaplayacak.
- SSAO da aynı shader içinde (veya ayrı bir pass olarak) hesaplanıp gölgelerde köşelerin kararması sağlanacak.

---

> [!IMPORTANT]
> **User Review Required:**
> SSGI ve SSAO, ekran kartını oldukça yoran efektlerdir. Bu yüzden başlangıçta yarım çözünürlükte (Half-Res) veya basit bir gürültü (Noise) filtresi ile uygulamayı planlıyorum. Bu yaklaşım performans/kalite dengesi açısından size uygun mu? Yoksa tam çözünürlüklü daha ağır bir SSGI mı tercih edersiniz?

---

## Doğrulama Planı
1. Shaderlar ve C++ kodları yazılıp derlenecek.
2. Editör açılacak ve köşeli/kapalı bir oda tarzı geometri eklenecek.
3. Doğrudan ışık almayan köşelerin SSAO ile karardığı ve parlak zeminlerden seken ışığın (SSGI) duvarları hafifçe aydınlattığı gözlemlenecek.

Onay verdiğiniz an, SSGI ve SSAO için MRT altyapısını kodlamaya başlayacağım.
