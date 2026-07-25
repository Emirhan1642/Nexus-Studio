# TAA (Temporal Anti-Aliasing) İncelemesi

Faz 7.4 - Görev 8 kapsamında Motor'a TAA (Geçici Kenar Yumuşatma) sistemini başarıyla entegre ettik.

## Neler Yapıldı?

1. **Halton Jittering:** 
   - Kameranın Projeksiyon matrisi, her karede (frame) Halton (2 ve 3 tabanlı) dizisi kullanılarak alt-piksel (sub-pixel) seviyesinde rastgele kaydırıldı (Jittering). Bu, ekrandaki piksellerin her karede hafifçe farklı bir noktayı örneklemesini sağlayarak zaman içerisinde inanılmaz pürüzsüz kenarlar elde etmemizi sağlar.
   - Post-Process (SSGI, SSR vb.) hesaplamalarının bu kaydırmadan etkilenip bozulmaması için, `u_unjitteredInvProj` matrisi sisteme dahil edilerek ekran-uzayı işlemlerinin orijinal (titrememiş) matris üzerinden yürümesi sağlandı.

2. **TAA Shader (`fs_taa.sc`) ve Ping-Pong Buffer:**
   - Önceki karenin (History) TAA çıktısını tutmak için iki adet `m_taaFB` oluşturuldu ve bu bufferlar arasında her karede geçiş (Ping-Pong) yapıldı.
   - Piksellerin dünya üzerindeki pozisyonu kullanılarak bir önceki karedeki (History) ekran koordinatı (Velocity) hesaplandı. 
   - History rengi mevcut rengin 3x3 komşuluk piksellerinin Min/Max renk limitleri arasına sıkıştırılarak (Neighborhood Clamping) hayalet izi (Ghosting) büyük ölçüde önlendi.

3. **FXAA Uyumluluğu:**
   - TAA ile titreşimler engellendiği ve kenarlar düzeltildiği için çoğu zaman FXAA gereksiz kalır. Ancak ileride eklenecek *Editor Settings* paneliyle kontrol edilebilmesi için sistemde aktif bırakıldı.

## Doğrulama
- Shader kodları `shaderc` ile sorunsuz derlendi.
- C++ motoru `m_taaIndex` ve matris tersini alma işlemleri düzeltildikten sonra hatasız şekilde (0 Error) derlendi.
- Kamera hareketlerinde eski kareleri referans alarak daha stabil ve pürüzsüz görüntüler elde edildi.

**Test Etmek İçin:** `build/bin/Debug/NexusStudioEditor.exe` çalıştırarak, özellikle ince hatlara sahip objelere yaklaşarak kamerayı hafifçe hareket ettirin. Titreme ve aliasing sorunlarının (SSGI noise'ları dahi) nasıl stabilize olduğunu gözlemleyebilirsiniz.
