# Faz 2: Render Pipeline (DataModel'den Ekrana)

Bu fazda, Faz 1'de inşa ettiğimiz `DataModel` (özellikle `Part`) nesnelerini bgfx üzerinden ekrana çizdireceğiz. Performans ve mimari doğruluğu sağlamak için "Render Scene" (Düz Proxy dizisi) yöntemini kullanacağız.

## User Review Required

> [!IMPORTANT]
> bgfx, kendi özel shader dili olan `.sc` uzantılı dosyalar kullanır. Bu shader'ların çalışma zamanından önce `shaderc` aracıyla derlenmesi gerekmektedir. Bunun için CMake konfigürasyonumuzu `shaderc` aracını derleyecek şekilde (`BGFX_BUILD_TOOLS=ON`) güncelleyeceğim.

> [!TIP]
> MVP (Minimum Viable Product) için, karmaşık doku (texture) yükleme sistemlerini bu faza dahil etmeyip, PBR materyallerini (Albedo, Metallic, Roughness) sadece sabit değerler (uniforms) üzerinden yöneteceğiz. Doku (Asset) yükleme sistemi ileriki fazlarda genişletilebilir.

## Open Questions

- Render edilecek basit bir Küp (Cube) modeli (vertex/index verisi) için geçici bir hardcoded mesh üreteceğim. Bu yaklaşım başlangıç için uygun mu?

## Proposed Changes

### 1. Math Modülü Genişletmesi
- **[NEW]** `Engine/Core/Math/Matrix4.h` & `.cpp`: 4x4 Matris işlemleri (Perspective, LookAt, Translation vb.)

### 2. Renderer Modülü (Yeni)
- **[NEW]** `Engine/CMakeLists.txt`: `EngineRenderer` kütüphanesini CMake'e ekleme
- **[NEW]** `Engine/Renderer/SceneGraph/RenderProxy.h`: Çizilecek nesnenin düz (flat) GPU verisi
- **[NEW]** `Engine/Renderer/SceneGraph/RenderScene.h` & `.cpp`: Proxy'leri düz bir vektörde tutan sahne yöneticisi
- **[NEW]** `Engine/Renderer/Camera.h`: Free-fly kamera matrislerini sağlayan sınıf
- **[NEW]** `Engine/Renderer/Renderer.h` & `.cpp`: Culling, sıralama ve bgfx draw call'larını yöneten ana sınıf

### 3. DataModel Güncellemeleri
- **[MODIFY]** `Engine/Core/DataModel/Instance.h`: `onAddedToWorkspace` ve `onRemovedFromWorkspace` sanal metodlarının eklenmesi.
- **[MODIFY]** `Engine/Core/DataModel/Part.h` & `.cpp`: Property (Position vb.) değiştiğinde `RenderScene` üzerinden kendi `RenderProxy`'sini "dirty" olarak işaretlemesi.

### 4. Shaders ve CMake Pipeline
- **[MODIFY]** `ThirdParty/CMakeLists.txt`: `BGFX_BUILD_TOOLS=ON` yapılması.
- **[NEW]** `Engine/Renderer/Shaders/vs_pbr.sc` ve `fs_pbr.sc`: Temel PBR (Albedo/Metallic/Roughness) shader'ları.
- **[NEW]** `Engine/Renderer/CMakeLists.txt`: `shaderc` kullanarak `.sc` dosyalarından `.bin` (derlenmiş shader) üreten özel CMake hedefleri.

### 5. Editor Entegrasyonu
- **[MODIFY]** `Editor/Main.cpp`: `Renderer` sınıfının başlatılması, boş pencerenin yerini içi dolu render döngüsünün alması. Kamera kontrollerinin eklenmesi (basit mouse/klavye takibi).

## Verification Plan

### Automated Tests
- Eklenen `Matrix4` işlemleri için yeni birim testleri eklenecek.
- `Part` oluşturulduğunda ve yok edildiğinde `RenderScene` üzerinde ilgili proxy'lerin oluşup silindiğini test eden birim testler.

### Manual Verification
- `NexusStudioEditor.exe` çalıştırıldığında ekranda küplerden oluşan bir sahnenin görünmesi.
- Sabit ışık altında PBR parametrelerinin (örn. parlak ve pürüzlü iki farklı küp) görsel olarak doğrulanması.
- Kamera ile WASD kullanılarak sahne içinde gezinebilme.
