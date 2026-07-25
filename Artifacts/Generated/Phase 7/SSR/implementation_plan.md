# Faz 7.4 - Görev 7: SSR (Screen Space Reflections) Uygulama Planı

SSR (Ekran Uzayı Yansımaları), su birikintileri, ıslak zeminler veya metalik objeler gibi yansıtıcı yüzeylerin çevrelerini ekran uzayı derinliği üzerinden ışın izleyerek yansıtmasını sağlar. 

## 1. Mimari Tasarım
Yansıma işleminin objelerin pürüzlülük (Roughness) değerine göre hesaplanabilmesi için G-Buffer üzerinden Roughness verisine ulaşmamız gerekmektedir. Mevcut mimarimizde Forward Rendering yapıyor olsak da, SSGI için ayırdığımız `m_hdrFB`'nin ikinci renk karesi (`Normal Buffer - RGBA8`) boş bir Alpha kanalına sahiptir. Roughness verisini bu Alpha kanalına paketleyeceğiz.

Efekt Sıralaması:
`Main Pass` -> `SSGI Pass` -> **`SSR Pass`** -> `DOF Pass` -> `Motion Blur Pass` -> `Tonemap` -> `FXAA`

SSR yansımaları, SSGI tarafından ortam ışığı eklenmiş görüntüyü (`m_ssgiFB`) okuyarak yapacak, böylece yansımalar çok daha gerçekçi olacaktır.

## 2. G-Buffer Güncellemesi (`fs_pbr.sc`)
- Normal verisi `gl_FragData[1]` içerisine yazılırken Alpha kanalı 1.0 olarak sabitlenmişti.
- `gl_FragData[1] = vec4(N * 0.5 + 0.5, roughness);` şeklinde güncellenerek pürüzlülük verisi SSR ve gelecekteki post-processler için G-Buffer'a eklenecek.

## 3. C++ Tarafı (`Renderer.h` & `Renderer.cpp`)
- `RenderView` Enum içerisine `View_SSR = 34` eklenecek.
- `bgfx::FrameBufferHandle m_ssrFB` (RGBA16F) ve `bgfx::ProgramHandle m_ssrProgram` eklenecek.
- Uniformlar: `u_ssrParams` (maxSteps, rayStep, thickness, threshold) eklenecek.
- `renderFrame` içerisinde SSGI pass'inden hemen sonra çalışacak şekilde SSR pass'i eklenecek.
- DOF pass'i artık girdi olarak `m_ssgiFB` yerine `m_ssrFB`'yi alacak.

## 4. SSR Shader'ı (`fs_ssr.sc`)
- **Girdiler:** 
  - `s_texColor` (SSGI Color)
  - `s_texNormalGBuffer` (RGB = Normal, A = Roughness)
  - `s_texDepth` (HDR Depth)
- **Mantık:**
  1. Yüzeyin `roughness` değeri belli bir eşiğin üzerindeyse (örn. 0.8) yansıma yapılmaz (erken çıkış).
  2. Kamera bakış vektörü ve yüzey normali kullanılarak `reflect()` ile yansıma vektörü (View-Space) bulunur.
  3. Yansıma vektörü yönünde Screen-Space Raymarching uygulanır (Örn: 20-30 adım).
  4. Çarpışma bulunursa, çarpılan noktanın UV'sindeki renk (`s_texColor`) yansıma rengi olarak alınır.
  5. Roughness'a bağlı olarak Fresnel hesaplanarak orijinal renk ile yansıma rengi birleştirilir.

---

> [!IMPORTANT]
> **Kullanıcı İncelemesi / Açık Sorular:**
> 1. Roughness bilgisini Normal haritasının Alpha kanalına paketleyerek G-Buffer optimizasyonu sağlama planını onaylıyor musunuz? (Bu yöntem AAA motorlarda standart bir yaklaşımdır).
> 2. Performans için ışın izleme (Raymarching) adımlarını 20 ile sınırlandırıp Binary Search kısmını şimdilik (MVP için) es geçmeyi planlıyorum. Sadece Linear Raymarching yapılacak. Bu kabul edilebilir mi?
