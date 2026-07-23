# Asset Browser & Import Akışı (Faz 4/Aşama 3B)

Bu plan, `Asset_Browser_Import_Akisi.md` dokümanındaki gereksinimler doğrultusunda arka plan dosya izleme, asset aktarımı (import), önbellekleme (thumbnail cache) ve UI üzerinden etkileşimli "Asset Browser" panelinin projeye tam entegre edilmesini kapsamaktadır. Mevcut durumda bazı dosyalar hazır olsa da arka plan bağlamaları yapılmamıştır.

## User Review Required

> [!IMPORTANT]
> Mevcut `ThumbnailRenderer` sadece placeholder dönmektedir. Gerçek bir mini render işlemi (offscreen render-to-texture) uygulamak için 128x128'lik bir FrameBuffer kullanacağım. Bu, Editör'de gördüğümüz asset ikonlarının gerçekten o modelin kendisi olmasını sağlayacaktır.

## Open Questions

> [!WARNING]
> Asset Hot-Reload (dosya dışarıda değiştiğinde sahnedeki mesh'in de güncellenmesi) işlemi için `efsw` dosya izleyicisi entegre edilecek. Bu işlem sırasında `Part` objeleri üzerindeki `RenderProxy`'nin baştan render edilmesi gerekecek. `Part::updateMeshHandle` gibi bir fonksiyon kullanmayı planlıyorum, onaylıyor musunuz?

## Proposed Changes

---

### Engine/Assets

#### [MODIFY] [FileWatcher.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/FileWatcher.h) / [FileWatcher.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/FileWatcher.cpp)
- `efsw` (Entropia File System Watcher) kütüphanesini projede aktif olarak başlatacak şekilde `AssetImportPipeline` üzerinden çalıştırılması.

#### [MODIFY] [AssetImportPipeline.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetImportPipeline.cpp)
- `initialize()` içerisinde `FileWatcher`'ın başlatılması ve `Assets` klasörünün dinlemeye alınması.
- `notifyDependentInstances` içinde `AssetDependencyTracker` kullanılarak, değişen asset'in tüm Instance'larda güncellenmesi (`hot-reload` entegrasyonu).

#### [MODIFY] [ThumbnailRenderer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/ThumbnailRenderer.cpp)
- `ThumbnailRenderer::renderThumbnail` içinde, eğer objenin bir mesh olduğu tespit edilirse `bgfx` kullanarak küçük bir sahne kamerası ile `RenderProxy`'nin geçici bir Framebuffer'a çizilmesi ve TextureHandle'ının cache'e alınması.

### Engine/Core/DataModel

#### [MODIFY] [Part.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.h) / [Part.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.cpp)
- `setMeshFromAsset` fonksiyonunun `AssetDependencyTracker::instance().registerUsage` çağırmasının sağlanması.
- `updateMeshHandle(bgfx::VertexBufferHandle vbh, bgfx::IndexBufferHandle ibh)` (veya uygun mesh handle'ı) metodu eklenerek RenderProxy'nin güncellenmesi.

### Editor/UI

#### [MODIFY] [AssetBrowserPanel.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/AssetBrowserPanel.cpp)
- Dosyaların gerçekten icon'larının görünüp sürükle-bırak olayının eksiksiz çalışır hale getirilmesi (mevcut durumda temelleri var, test edilip hataları giderilecek).

## Verification Plan

### Automated Tests
- Mevcut derleme sürecinin bozulup bozulmadığını kontrol edeceğiz.

### Manual Verification
1. **Import:** `Assets` klasörüne yeni bir obj (`mesh.obj`) dosyası eklenecek, editörün donmadan import işlemini arkada yapıp AssetBrowser'da Thumbnail'ını çıkardığı kontrol edilecek.
2. **Hot Reload:** Obj dosyası Blender'da değiştirilip kaydedilecek, sahnedeki objenin şeklinin anında değiştiği gözlenecek.
3. **Sürükle-Bırak:** Asset Browser üzerinden sürüklenen mesh'in Viewport'a başarıyla atılabilmesi.
