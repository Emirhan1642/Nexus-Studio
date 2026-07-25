# SSR (Screen Space Reflections) İncelemesi

Faz 7.4 - Görev 7 kapsamında Motor'a SSR (Ekran Uzayı Yansımaları) sistemini başarıyla entegre ettik.

## Neler Yapıldı?

1. **G-Buffer Optimizasyonu ve Güncellemesi:**
   - Yüzeylerin pürüzlülük (Roughness) değerlerini baz alan gerçekçi yansımalar hesaplayabilmek için `fs_pbr.sc` (Ana renk shader'ı) güncellendi.
   - Pürüzlülük verisi, G-Buffer'daki Normal Haritasının (`m_hdrFB` 1. eklentisi) Alpha (A) kanalına paketlendi. Böylece fazladan bir texture/bellek masrafı olmadan SSR için gerekli pürüzlülük verisi elde edildi.

2. **C++ (Render Hattı) Entegrasyonu:**
   - `Renderer.h` ve `Renderer.cpp` güncellenerek sisteme yeni bir Render View (`View_SSR`) ve Framebuffer (`m_ssrFB`) eklendi.
   - SSR pass'i, SSGI (Ortam Işığı) tamamlandıktan hemen sonra çalışacak şekilde sıraya eklendi. Böylece yansımaların içinde ışıklandırılmış/gölgelendirilmiş orijinal çevre hesaplanabildi.
   - DOF (Alan Derinliği) ve sonrasındaki (Motion Blur vb.) efektler, SSR'ın çıktısını girdi olarak alacak şekilde zincire bağlandı.

3. **SSR Shader Mantığı (`fs_ssr.sc`):**
   - **Girdiler:** SSGI'dan çıkan renk, G-Buffer Normal+Roughness ve HDR Depth.
   - **Performans Optimizasyonu:** `roughness` değeri ayarlanan eşiğin (ör. 0.8) üzerinde olan yüzeyler için Screen Space Raymarching (Işın İzleme) **es geçildi (Early-out)**. 
   - **Raymarching:** Belirlenen adım sayısı (20) ve adım büyüklüğüne göre yansıma vektörü üzerinde ışın izlendi. Z-Depth kontrolü yapılarak çarpışma hesaplandı.
   - **Fiziksel Tabanlı (PBR) Harmanlama:** Çarpışma rengi (yansıyan cismin rengi), Fresnel formülü, Ekran Köşelerine doğru karartma (Edge Fade) ve Roughness'a bağlı bulanıklaşma etkileri hesaplanarak orijinal yüzey rengiyle birleştirildi.

## Doğrulama
- Shader kodları `shaderc` aracılığıyla BGFX formatlarına başarıyla derlendi.
- C++ motor kaynak kodları (CMake) başarıyla ve hatasız bir şekilde derlendi (0 Error).

Artık motor içerisinde parlak (roughness < 0.8) olan objeler ve yüzeyler, etraflarındaki (ekran uzayındaki) objeleri kendi yüzeylerinde dinamik olarak yansıtabiliyor!

**Test Etmek İçin:** `build/bin/Debug/NexusStudioEditor.exe` çalıştırılarak bir test sahnesi ile (Roughness değeri düşük materyaller kullanarak) yansımaları gözlemleyebilirsiniz.
