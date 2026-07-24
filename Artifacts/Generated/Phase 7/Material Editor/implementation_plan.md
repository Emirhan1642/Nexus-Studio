# Faz 7.1 - Node-Based Materyal Editörü

Bu aşamada "Veri Odaklı" yaklaşımımızı görsel bir editöre taşıyıp, Unreal Engine tarzı bir Shader Graph altyapısı kuracağız.

## Mimari Yaklaşım

1. **Kullanıcı Arayüzü (UI):** 
   - Grafiksel düğüm (node) arayüzü için hafif ve ImGui ile uyumlu olan [Nelarius/imnodes](https://github.com/Nelarius/imnodes) kütüphanesini `FetchContent` ile entegre edeceğiz.
2. **Veri Modeli (`ShaderGraph`):**
   - Bellekte düğümleri ve bu düğümler arasındaki bağlantıları tutacak bir veri yapısı oluşturulacak (Bkz. `ShaderGraph.h`).
3. **Derleyici (`ShaderGraphCompiler`):**
   - Kullanıcının kurduğu grafiği (topolojik sıralama ile) GLSL / BGFX `.sc` formatına çevirecek.
   - BGFX'in `shaderc` aracı çalışma zamanında (runtime) veya düzenleme sonrasında çağrılarak bu kodu GPU'nun okuyabileceği `.bin` formatına derleyecek.
4. **Entegrasyon:**
   - Derlenen bu shader, sahnedeki objelerin `Material` bileşenine anında uygulanarak canlı önizleme (Live Preview) sağlayacak.

## Önerilen Değişiklikler

### 1. Kütüphane Entegrasyonu (`CMakeLists.txt`)
- `imnodes` kütüphanesi `ThirdParty` klasörüne eklenecek ve `Editor` modülüne bağlanacak.

### 2. Veri Modeli ve UI
- `Editor/Panels/MaterialEditorPanel.h/cpp` dosyaları eklenecek.
- `ShaderGraph` yapısı tasarlanacak: `ShaderNode` (Tip, Giriş/Çıkış pinleri, Veriler), `ShaderLink` (Pin bağlantıları).
- UI üzerinden sağ tık menüsüyle (Texture Sample, Multiply, Add, Constant vb.) düğümler eklenebilecek.

### 3. Shader Derleme Aşaması
- Grafikten (Graph) `.sc` (bgfx shader code) metni üretme algoritması (`ShaderGraphCompiler`).
- Üretilen metni `shaderc` komutuyla `.bin`'e dönüştürüp `RendererSystem` üzerinden `bgfx::ProgramHandle` olarak yükleme mantığı kurulacak.

---

> [!IMPORTANT]
> **User Review Required:**
> Node-based editörün çalışma anında derleme (runtime compilation) yapabilmesi için arka planda `shaderc` aracını çağırması gerekecek. Bu durum projenin `bin/` klasöründeki `shaderc.exe`'ye doğrudan erişimini gerektiriyor. MVP (Başlangıç) aşamasında, her node değiştiğinde değil, kullanıcı **"Derle ve Uygula" (Compile & Apply)** butonuna bastığında bu derleme işleminin yapılmasını öneriyorum. Bu yaklaşımı onaylıyor musunuz?

## Doğrulama Planı
1. Editörde yeni bir "Material Editor" penceresi açılacak.
2. Düğümler (Nodes) sürükle-bırak yöntemiyle birbirine bağlanabilecek.
3. "Compile" butonuna basıldığında geçerli bir Bgfx shader kodu üretilip, model üzerinde görsel sonuç anında doğrulanacak.

Lütfen planı inceleyip onaylayın, ardından CMake entegrasyonuyla başlayalım.
