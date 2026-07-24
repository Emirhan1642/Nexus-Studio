# Aşama 3b (Asset Browser Import Akışı) İmplementasyon Planı

Kullanıcının Faz 4'te arayüz iskeleti olarak oluşturulan `AssetBrowserPanel` ve altyapısı kurulan `AssetImportPipeline` sistemlerini, **Gerçek FBX Import (Skeletal Mesh) ve Doku (Texture)** mekanizmaları ile bağlama hedefidir. FBX sürükleyip bırakıldığında, veya dışarıdan bir dosya klasöre eklendiğinde sistem bunu otomatik algılayacak, asenkron olarak okuyacak, DataModel tarafındaki sahnede bulunan `Part`'lar bu assetleri kullandığında "Hot-Reload" ile kendilerini güncelleyecektir.

## User Review Required

> [!IMPORTANT]
> Bu aşama, motorun dosya sistemi ile UI arasında önemli bir köprü kuracaktır. Geliştirmeye başlamadan önce lütfen bu dokümandaki adımları inceleyin ve onay verin. MVP seviyesi için Assimp üzerinden elde edilecek Vertex ve Index buffer'ların doğrudan bgfx belleğine (VRAM) yüklenmesi sağlanacaktır. Gelişmiş bir ".nxmesh" binary serialization aşaması bu sürümde karmaşıklığı azaltmak için atlanacaktır.

## Open Questions

> [!WARNING]
> Asset Browser üzerinden properties paneline sürükle-bırak yaparken (örneğin Part'ın AlbedoTexture özelliğine), Part'ın bu asset değişimini nasıl dinlemesini istersiniz? Ben `AssetDependencyTracker` vasıtasıyla objelerin register olup dinlemesi yöntemini planlıyorum, bu size uygun mu?

## Proposed Changes

Aşağıda yapılacak değişiklikler bileşen bazlı gruplandırılmıştır:

---

### Asset Pipeline & Import Logic

#### [MODIFY] `Engine/Assets/AssetImportPipeline.cpp`
- `runImporterForFile` metodu içindeki sahte `sleep_for` kaldırılarak, gerçek dosya formatı (uzantı) kontrolü eklenecek.
- `.fbx` dosyaları için `SkeletalMeshImporter::importFBX` çağrılarak FBX verisi okunacak, AssetDatabase içerisine bellekte `ImportedSkeletalMesh` tutulacak.
- Diğer formattaki texture dosyaları (`.png`, `.jpg`) için gerekli tag'ler oluşturulacak.

#### [MODIFY] `Engine/Assets/ThumbnailRenderer.cpp`
- Mesh'ler için her ne kadar gerçek bir raytracing stüdyo aydınlatması MVP'de ağır olsa da, `RendererSystem::renderSingleObject` benzeri bir mantıkla oluşturulan 128x128 framebuffer içine asıl objenin geometrisi çizdirilip thumbnail olarak dönülmesi denenecektir. Texture'lar için zaten doğrudan resim döndürülmektedir.

---

### Editor UI

#### [MODIFY] `Editor/UI/AssetBrowserPanel.cpp`
- Arka planda import işlemi devam ediyorsa, `ImGui::OpenPopup` veya bir Progress Bar vasıtasıyla kullanıcıya `ImportProgressOverlay` gösterilecek. (C.2 İlerleme Göstergesi)

#### [MODIFY] `Editor/UI/PropertiesPanel.cpp`
- Sürükle-bırak payload'u olan `ASSET_GUID` bilgisini yakalama özelliği eklenecek.
- Eğer property bir `std::string` tipindeyse ve Texture yolu bekliyorsa (veya yeni oluşturacağımız AssetHandle tipi), Properties Panel üzerine Guid bırakıldığında, GUID'in relative path'i resolve edilip objeye yazılacak ve Undo/Redo stack'ine işlenecek.

---

### DataModel (Hot-Reload Entegrasyonu)

#### [MODIFY] `Engine/Core/DataModel/Part.cpp` & `Part.h`
- `meshAssetGuid` adlı bir özellik `Part` objesine eklenebilir veya var olan `AlbedoTexture` gibi yollar üzerinden `AssetDependencyTracker`'a objenin kaydolması (`registerUsage`) sağlanacak.
- `AssetImportPipeline` tarafında FBX tekrar değişip import edilirse, `AssetDependencyTracker::getUsers` ile o dosyayı kullanan tüm objelere bildirim gidecek ve mesh buffer'ları canlı olarak güncellenecek.

## Verification Plan

### Manuel Doğrulama
- Projenin `Assets` klasörüne dışarıdan bir `character.fbx` dosyası kopyalanacak.
- Editör anında dosya değişikliğini yakalayıp Import Pipeline'a sokacak.
- Asset Browser Panel'de bu dosya bir Asset Kartı olarak belirecek ve küçük bir Thumbnail'i hesaplanmış olacak.
- Bu kart, sahnedeki bir objenin (veya Properties panelindeki özelliğinin) üstüne sürüklendiğinde anında sahnede karakter mesh'i belirecektir.
- Dosya başka bir programda değişip kaydedildiğinde, sahnedeki objede anında görsel güncelleme ("hot-reload") tetiklenecektir.
