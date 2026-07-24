# Walkthrough: Shadow Mapping (Gölgeleme) Özelliği

Nexus Studio oyun motorunun Aşama 2 (Render Pipeline) planı kapsamında **Gölgeleme (Shadow Mapping)** özelliği MVP (Minimum Viable Product) olarak `RendererSystem` içerisine başarıyla entegre edilmiştir. 

> [!NOTE]
> Temel gölge sistemi (Shadow Map), özellikle animasyonlu karakterler ve sahnede derinlik hissi yaratmak için oyun motorlarında olmazsa olmaz bir sistemdir.

## Yapılan Değişiklikler

### 1. Shader Altyapısı (BGFX - GPU)
*   **Shadow Pass Shader'ları:** 
    *   `vs_shadow.sc` ve `vs_skinned_shadow.sc` oluşturuldu. Bunlar sahnedeki tüm objeleri sadece **Güneş Işığının** (Directional Light) açısından (kamerasından) çizerek derinlik değerlerini alır.
    *   `fs_shadow.sc` ile bu veriler, Donanımsal Derinlik Tamponu (Hardware Depth Buffer) kullanılarak kaydedilir.
*   **PBR Shader Güncellemeleri:** 
    *   Ana shader olan `vs_pbr.sc` ve `vs_skinned_pbr.sc` içerisine `u_lightMtx` matrisi eklendi ve her bir pikselin ışık uzayındaki koordinatları (`v_posLightSpace`) hesaplanıp fragment shader'a aktarıldı.
    *   `fs_pbr.sc` içerisine **3x3 PCF (Percentage-Closer Filtering)** yumuşatmalı gölge harmanlaması eklendi. Bu sayede objelerin kenarlarındaki gölge hatları tırtıklı değil, daha yumuşak ve göze hoş görünür.

### 2. RendererSystem (CPU)
*   **İki Aşamalı Render (2-Pass Rendering):** `Renderer.cpp` içerisindeki `renderFrame()` fonksiyonu güncellenerek ekran çizimleri **iki geçişli** hale getirildi.
    1.  *Geçiş 1 (Shadow Pass):* Tüm sahne objeleri ışık açısından özel bir FrameBuffer'a (Shadow Map `D16` texture) çizilir.
    2.  *Geçiş 2 (Main Pass):* Sahnede görünen objeler ana kamera açısından çizilir ve bu çizim sırasında Geçiş 1'den gelen gölge haritası okunarak objelerin aydınlatma değeri kısılır (Gölge efekti oluşturulur).
*   **Bellek ve Kaynak Yönetimi:** Gölge haritası (Shadow Map Framebuffer) `shutdown()` aşamasında sızıntı yapmaması için temizlenmek üzere bellek yönetimine eklendi.
*   `compile_shaders.bat` dosyası yeni shaderları sorunsuz derleyecek şekilde güncellendi.
*   Tüm CMake ve BGFX bağımlılıkları başarılı bir şekilde derlendi.

## Nasıl Test Edilir?
Nexus Studio Editör uygulamasını başlatarak projenizdeki ana sahneye bakabilirsiniz:
1. Sahnede bulunan Humanoid karakteri ve standart Mesh bileşenleri (Küp vb.) yer düzlemi üzerinde artık güneşin geliş açısına göre karanlık ve belirgin gölgeler düşürecektir.
2. Animasyon oynatıldığında kolların ve bacakların hareketleri, yere düşen gölge üzerinde **gerçek zamanlı (GPU Skinning ile uyumlu)** olarak gözlemlenebilir.

> [!TIP]
> Editör içinden projenizi başlatarak gölgeleri test edebilirsiniz. Sonraki hedef olarak eğer planlar dâhilinde ise Post-Processing veya bir başka sistemi inceleyebiliriz. Eksikler dosyasına göre sıradaki adıma geçmeye hazırım.
