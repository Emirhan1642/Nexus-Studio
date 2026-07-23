# Asset Browser & Import Akışı (Aşama 3/B)

Bu plan, `Asset_Browser_Import_Akisi.md` dokümanındaki gereksinimler doğrultusunda arka plan dosya izleme, asset aktarımı (import), önbellekleme (thumbnail cache) ve UI üzerinden etkileşimli "Asset Browser" panelinin projeye entegre edilmesini kapsamaktadır.

## User Review Required

> [!IMPORTANT]
> **Üçüncü Parti Kütüphaneler:** Dokümanda bahsedilen `.meta` (JSON) dosyalarını ayrıştırmak için `nlohmann_json` ve platform bağımsız dosya izleme için `efsw` kütüphanelerini `CMakeLists.txt`'ye (FetchContent ile) eklemeyi planlıyorum. EFSW'nin derlenmesi bazen C++ ortamlarında ekstra ayar gerektirebiliyor.
> **Onaylıyor musunuz?** (Eğer EFSW çok ağır derseniz ilk aşamada standart kütüphane olan `std::filesystem::last_write_time` ile basit bir polling mekanizması kurabiliriz.)

> [!WARNING]
> **Thumbnail Rendering (BGFX):** Thumbnail'ler asenkron olarak küçük frame-buffer'lara render edilip önbelleklenecek. İlk açılışta kasmayı önlemek için "frame başına maksimum 2 thumbnail render" sınırı konulacaktır. 

## Open Questions

> [!TIP]
> Drag&Drop mekanizmasında Viewport'a sürüklenen mesh'lerin otomatik olarak bir `Part` oluşturması sağlanacak. Varsayılan materyal (PBR değerleri) atanmalı mı yoksa sadece Texture/Mesh mi yüklenmeli?

## Proposed Changes

---
### ThirdParty Dependencies

#### [MODIFY] [ThirdParty/CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/ThirdParty/CMakeLists.txt)
- `nlohmann_json` kütüphanesini FetchContent ile çek.
- `efsw` (Entropia File System Watcher) kütüphanesini FetchContent ile çek (Kullanıcı onaylarsa).

---
### Engine/Assets Modülü

#### [MODIFY] [Engine/CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/CMakeLists.txt)
- `add_subdirectory(Assets)` satırını ekle.

#### [NEW] [Engine/Assets/CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/CMakeLists.txt)
- Yeni `EngineAssets` kütüphanesini tanımla.

#### [NEW] [Engine/Assets/AssetDatabase.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetDatabase.h) & `.cpp`
- `AssetGuid` yapısı ve `.meta` dosyası okuma/yazma (JSON) mantığı eklenecek.

#### [NEW] [Engine/Assets/FileWatcher.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/FileWatcher.h)
- Klasördeki dosya değişikliklerini (silme, ekleme, değiştirme) dinleyecek arka plan watcher. 

#### [NEW] [Engine/Assets/AssetImportPipeline.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetImportPipeline.h)
- Değişen dosyaları yakalayıp worker thread'e atan, import tamamlandığında ana thread'e bildirim yollayan pipeline.

#### [NEW] [Engine/Assets/ThumbnailRenderer.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/ThumbnailRenderer.h)
- Sabit ışıkla (stüdyo) `bgfx::FrameBuffer` içerisine bir mesh'in önizlemesini çizen ve `bgfx::TextureHandle` döndüren sistem.

#### [NEW] [Engine/Assets/ThumbnailCache.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/ThumbnailCache.h)
- Thumbnail isteklerini kuyruğa alıp her frame sınırlı sayıda (throttle) render edilmesini sağlayan önbellek.

#### [NEW] [Engine/Assets/AssetDependencyTracker.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Assets/AssetDependencyTracker.h)
- Hangi `InstanceId`'nin hangi `AssetGuid`'i kullandığını takip eden sistem.

---
### DataModel ve Editör UI

#### [MODIFY] [Engine/Core/DataModel/Part.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.cpp)
- `setMeshFromAsset(AssetGuid guid)` metodu eklenecek, bu sayede Asset Dependency Tracker'a kayıt olunacak.

#### [NEW] [Editor/UI/AssetBrowserPanel.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/AssetBrowserPanel.h) & `.cpp`
- Thumbnail grid'ini çizen ve `ImGui::BeginDragDropSource("ASSET_GUID")` tetikleyen yeni arayüz paneli.

#### [MODIFY] [Editor/UI/ViewportPanel.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/UI/ViewportPanel.cpp)
- Viewport'ta `ImGui::BeginDragDropTarget()` uygulanacak. Bırakılan yere göre raycast yapılarak yeni `Part` eklenecek ve objenin mesh'i bu Asset ile değiştirilecek.

#### [MODIFY] [Editor/Editor.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/Editor.cpp)
- Ana menüye AssetBrowser sekmesi eklenecek ve UI başlatılırken instance kaydı oluşturulacak.

## Verification Plan
1. **Dosya İzleme (FileWatcher) Testi:** Windows Explorer üzerinden çalışma dizinine bir dosya bırakıp otomatik `.meta` oluştuğunu teyit etmek.
2. **Asenkron Thumbnail Render:** AssetBrowserPanel açıldığında grid üzerinde önizlemelerin FPS düşüşü yaşatmadan yavaş yavaş karelere render edildiğini görmek.
3. **Hot-Reload:** Arka planda mesh değiştirildiğinde editördeki `Part`'ların otomatik olarak güncellenmesi.
4. **Sürükle-Bırak:** Asset Browser üzerinden sürüklenen mesh'in Viewport'a başarıyla atılabilmesi.
