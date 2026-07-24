# FXAA (Fast Approximate Anti-Aliasing) Walkthrough

Bu aşamada motorun render mimarisine **FXAA** (Hızlı Yaklaşık Kenar Yumuşatma) algoritması entegre edilmiştir. Artık Post-Processing zinciri SDR renk kalibrasyonunu yaptıktan hemen sonra piksellenmiş kenarları (aliasing) yumuşatıp ekrana çizmektedir.

## Yapılan Değişiklikler

### 1. Render Zinciri ve FrameBuffer Düzenlemeleri
- `Renderer.h` içerisine `m_tonemapFB` eklendi. Önceden tonemapping işlemi doğrudan ekrana (Backbuffer) çizmekteyken, artık `m_tonemapFB` isimli SDR (RGBA8) hedefine çizmektedir.
- `View_Tonemap` görünümü güncellenerek hedefi `m_tonemapFB` olarak değiştirildi.
- `View_FXAA` isimli yeni bir pass (`ViewId = 31`) eklendi. Bu pass, `m_tonemapFB`'yi girdi olarak alıp nihai sonucu Backbuffer'a (ekrana) çizmektedir.
- Ekran yeniden boyutlandırma (`resize`) mantığına `m_tonemapFB`'nin oluşturulması ve güncellenmesi eklendi.

### 2. FXAA Shader'ı Geliştirmeleri
- `Engine/Renderer/Shaders/fs_fxaa.sc` dosyası oluşturuldu. 
- Standart luma (parlaklık) tabanlı kenar tespiti algoritması kullanılarak "Edge Threshold" kontrolleri ile pikseller etrafında kenar bulanıklaştırması sağlandı.
- Girdi olarak `s_texTonemap`, çözünürlük verisi olarak da `u_fxaaParams` (`x: 1/width`, `y: 1/height`) shader'a gönderildi.
- `compile_shaders.bat` güncellenerek bu yeni shader derlemeye dâhil edildi.

## Sonuç
Motor başarıyla derlendi. Görüntü Bloom -> Tonemapping (HDR to SDR) -> FXAA rotasını izleyerek ekrana çıkmaktadır. Ekrandaki keskin tırtıklanmaların azaldığını ve daha sinematik bir görüntü elde edildiğini gözlemleyebilirsiniz.
