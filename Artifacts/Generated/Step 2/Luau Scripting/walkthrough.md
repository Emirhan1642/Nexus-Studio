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

## Aşama 3: Luau Scripting & Integration

- **Luau Runtime**: `LuauVM` was created to initialize the Luau compiler and VM, apply Roblox-style sandboxing (removing `io`, `os`, `package`, `dofile`, etc.), and handle script execution securely.
- **Reflection Bindings**: Created `InstanceBinding` to connect our C++ `TypeRegistry` (from Phase 1) directly to Luau. This allows Lua code to read/write properties like `part.Position = part.Position + Vector3.new(...)` dynamically without manual C++ glue code per class.
- **Math Bindings**: Created `Vector3Binding` with full metatable operator support (addition, subtraction, multiplication, scaling).
- **Coroutine Scheduler**: Implemented `ScriptScheduler` to support the global `wait()` function in Lua. Scripts now yield execution (`lua_yield`) and automatically resume after the elapsed time, enabling asynchronous scripting identical to Roblox.
- **Script Class**: Created a `Script` Instance in the DataModel that compiles and runs its source code when added to the workspace.
- **Integration**: Updated `Editor/Main.cpp` to initialize the VM, tick the scheduler every frame, and run a test script that continuously moves a cube up.

> [!TIP]
> The engine now supports live script execution and property manipulation. Try running the Editor to see the script move `MyCube2` smoothly on every frame!
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
