# Dithered LOD Transitions Walkthrough

Bu aşamada motorun Render Pipeline'ına, uzaklığa bağlı değişen LOD (Level of Detail) modelleri için **Yumuşak Geçiş (Dithering)** özelliği entegre edilmiştir. Modellerin bir anda değişmesi (pop-in) yerine piksel bazında eriyerek (dissolve) diğerine geçmesi sağlanmıştır.

## Yapılan Değişiklikler

### 1. C++ Renderer Mantığı Güncellendi
- `Renderer.cpp` içerisindeki `renderFrame` döngüsüne geçiş bölgesi (Transition Range = 5.0 birim) mantığı eklendi.
- Bir obje LOD 0'dan LOD 1'e veya LOD 1'den LOD 2'ye geçerken, bu 5 birimlik geçiş mesafesi içerisinde *iki kere* çizilmesi (birisi eriyerek kaybolan, diğeri beliren obje) sağlandı.
- Aynı mantık **Shadow Map** (Gölge) render döngüsü için de uygulandı, böylece gölgeler de objeyle eş zamanlı olarak yumuşak şekilde geçiş yapmaktadır.
- Geçişin (fade) shader'a iletilebilmesi için `u_lodParams` uniformu eklendi.

### 2. Shader Entegrasyonu
- `fs_pbr.sc` (Ana materyal shader'ı) ve `fs_shadow.sc` (Gölge shader'ı) içerisine Dither (Erime) mantığı entegre edildi.
- Ekran koordinatları (`gl_FragCoord.xy`) tabanlı, modern oyun motorlarında yaygın olarak kullanılan **Interleaved Gradient Noise** algoritması kullanılarak pikseller rastgele ama düzgün bir desende (`discard` ile) silindi. (Şeffaflık/Alpha kullanılmadığı için Depth Buffer bozulmaz ve performans yüksektir).

### 3. Editor Settings İçin Notlar Alındı
- `eksikler_ve_iyilestirmeler.md` dosyası güncellenerek, UI eklendiğinde **LOD Geçiş Mesafesinin** ve **Gölge Dither** özelliğinin açılıp kapatılabileceği Editor Settings notları eklendi.

## Sonuç
Oyun motoru başarıyla derlendi. Artık kamerayı objeden uzaklaştırdığınızda (veya yaklaştırdığınızda), modeller aniden değişmek yerine 5 metrelik bir geçiş aralığında birbiri içerisinde eriyerek çok daha profesyonel ve pürüzsüz bir görüntü sunmaktadır.
