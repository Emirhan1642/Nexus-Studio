# Aşama 2: Render Pipeline Tamamlandı!

Bu aşamada DataModel'deki özellik değişikliklerinin doğrudan GPU'da nasıl temsil edildiğini gösteren **Render Proxy** mimarisini hayata geçirdik. C++ DataModel ağacını taramanın getirdiği darboğazları önlemek için bgfx'e verileri "düz (flat)" bir liste olarak iletiyoruz.

## Yapılan Değişiklikler

1. **Math Genişletmeleri:**
   - 4x4 Matris (`Matrix4.h` & `.cpp`) altyapısı yazıldı. Perspective, LookAt ve Transform fonksiyonları eklendi.

2. **Render Scene & Proxy Mimarisi:**
   - Ekranda çizilecek her nesnenin basitleştirilmiş bir kopyasını tutan `RenderProxy` struct'ı oluşturuldu.
   - Bu proxy'leri yöneten ve DataModel'den tetiklenen güncellemeleri iş parçacığı güvenli şekilde depolayıp, render döngüsü başında senkronize eden `RenderScene` inşa edildi.
   - Kamera işlemlerini sarmalayan `Camera` sınıfı hazırlandı.

3. **DataModel Entegrasyonu:**
   - `Instance` sınıfına Workspace'e eklenme/çıkarılma olayları için virtual fonksiyonlar eklendi.
   - `Part` sınıfı modifiye edilerek, oluşturulduğunda bir Render Proxy kaydetmesi (ve yok edildiğinde silmesi) sağlandı. Ayrıca özellik (position, size) değişimlerinde Reflection getter/setter'ları aracılığıyla asenkron şekilde `RenderScene` üzerinden kendisini **Dirty** (kirli/güncellenmesi gereken) olarak işaretlemesi ayarlandı.

4. **Shader Pipeline:**
   - `BGFX_BUILD_TOOLS` aktif edilerek bgfx'in kendi `shaderc` derleyicisinin projeyle birlikte derlenmesi sağlandı.
   - MVP kapsamında `vs_pbr.sc`, `fs_pbr.sc` ve `varying.def.sc` shader dosyaları yazıldı.
   - Kolay kullanım için `compile_shaders.bat` isimli otomatik derleme script'i oluşturuldu ve shader'lar HLSL profilinde (DirectX 11) derlendi.

5. **Editor Render Döngüsü:**
   - `NexusStudioEditor` uygulaması `RendererSystem` ile entegre edildi. Editor açılışında `DataModel` üzerine eklenen iki test küpünün, yazılan render pipeline'ından geçerek ekranda görünmesi sağlandı.

## Doğrulama
- CMake konfigürasyonu sorunsuz tamamlandı.
- Bütün bağımlılıklar ve `EngineRenderer` derlendi.
- `NexusStudioEditor.exe` başarıyla bağlandı (link) ve üretildi.

> [!NOTE]
> Editor'ü çalıştırmak için `build/bin/Debug/NexusStudioEditor.exe` dosyasını açabilirsiniz. Ekranda DataModel'den gönderilen 2 adet render proxy küpünü göreceksiniz. Shader'ların doğru konumdan yüklenebilmesi için komut satırından proje ana dizininde (`Nexus Studio` klasöründe) çalıştırılması önerilir.
