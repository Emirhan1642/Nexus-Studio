# Cascaded Shadow Maps (CSM) Walkthrough

Bu aşamada motorun render mimarisine **Cascaded Shadow Maps (CSM)** entegre edilmiştir. Bu geliştirme ile uzaklıklara göre farklı çözünürlükteki gölge haritaları kullanılarak piksellenme (perspective aliasing) sorunu çözülmüştür.

## Yapılan Değişiklikler

### 1. Motor Tarafı (C++)
- `m_shadowMapFB` tekil yapısı, `m_shadowMapFBs[3]` olarak 3 adet FrameBuffer (Cascade 0, 1, 2) tutacak şekilde genişletildi.
- Kamera view frustum'u `15.0m`, `50.0m` ve `150.0m` mesafelerinde 3 ayrı Cascade bölgesine ayrıldı.
- Her cascade için ışığın bakış açısından projeksiyon ve bounding box ortografik matrisleri hesaplandı. `u_lightMtx` (3 adet matris içeren array) ve `u_csmParams` (bölünme mesafelerini içeren vec4) olarak shaderlara gönderildi.
- Gölge çizim (Shadow Pass) işlemi, 3 cascade üzerinden çalışacak şekilde ayrıştırılarak 3 farklı `ViewId` (View_ShadowCascade0, View_ShadowCascade1, View_ShadowCascade2) ile işleme sokuldu.
- `Renderer.cpp` içerisindeki FrameBuffer destroy işlemleri yeni array sistemine uygun olarak güncellendi.

### 2. Shader Tarafı (BGFX)
- `varying.def.sc` içerisindeki `v_posLightSpace` yerine `v_viewDepth` eklendi. Bu sayede derinliğe göre (Z değeri) cascade seçimi yapılabilmektedir.
- `vs_pbr.sc` dosyasında `v_viewDepth`, vertex'in View space Z değerine (`mul(u_view, ...).z`) atanarak Fragment Shader'a gönderildi.
- `fs_pbr.sc` dosyasında 3 adet ayrı sampler (`s_texShadow0`, `s_texShadow1`, `s_texShadow2`) eklendi.
- `fs_pbr.sc` içerisinde, `v_viewDepth` ile piksellerin hangi cascade içerisinde olduğu (`cascadeIdx`) tespit edilerek doğru sampler üzerinden 3x3 PCF yumuşak gölge örneklemesi yapıldı.
- Shaderlar `compile_shaders.bat` çalıştırılarak başarıyla derlendi.

## Sonuç
Motor başarıyla derlendi ve tüm gölge sistemi 3 kademeli, çoklu-pass mimarisine uygun şekilde çalışır duruma getirildi. VCT Global Illumination sistemi ve CSM, Post-Processing katmanından (Bloom, HDR, vb.) hemen önce entegre biçimde çalışmaktadır.
