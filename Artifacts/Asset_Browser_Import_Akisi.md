# Asset Browser / Import Akışı — Teknik Derinlemesine İnceleme
## Dosya Sisteminden Editöre: AssetDatabase, Thumbnail'lar ve Hot-Reload

Bu doküman, Faz 2'nin Assets/Importers modülünde (mesh import) ve Skeletal Animation dokümanının Bölüm E'sinde (FBX import) tanımlanan **arka plan** import mantığının, editörde görsel ve etkileşimli bir panele nasıl dönüştürüleceğini inceler. Hedef: Kullanıcının bir `.fbx` dosyasını proje klasörüne sürükleyip bıraktığında, editörde otomatik olarak beliren, tıklanıp sahneye sürüklenebilen bir asset kartı görmesi.

---

## Bölüm A — AssetDatabase: Dosya Sistemi ile Motor Arasındaki Köprü

### A.1 Neden ham dosya yolu yeterli değil

İlk akla gelen naif yaklaşım, bir Part'ın mesh'ini doğrudan dosya yoluyla (`"C:/MyProject/Assets/character.fbx"`) referans etmesi olurdu. Bu, birkaç ciddi soruna yol açar:

- **Taşınabilirlik:** Proje başka bir bilgisayara kopyalandığında mutlak yollar (`C:/Users/Ahmet/...`) bozulur.
- **Yeniden adlandırma:** Kullanıcı dosyayı yeniden adlandırdığında veya taşıdığında, o dosyaya referans veren tüm Part'lar bağlantısını kaybeder.
- **Import ayarları nerede saklanacak:** Bir mesh'in LOD ayarları, sıkıştırma kalitesi gibi meta veriler dosyanın kendisinde saklanamaz (özellikle .fbx gibi motorun kontrol etmediği formatlarda).

**Çözüm: Her asset'e, dosya yolundan bağımsız, kalıcı bir GUID atanır.** Bu, Unreal ve Unity'nin de kullandığı standart yaklaşımdır.

```cpp
// Engine/Assets/AssetDatabase.h

struct AssetGuid {
    uint64_t high, low; // 128-bit GUID

    bool operator==(const AssetGuid& other) const { return high == other.high && low == other.low; }
};

struct AssetMetadata {
    AssetGuid guid;
    std::string relativePath;     // Proje köküne göre göreceli yol — taşınabilirlik için
    std::string importerType;     // "SkeletalMesh", "Texture", "Sound" vb.
    uint64_t sourceFileHash;      // Dosya değişti mi kontrolü için (Bölüm D)
    std::any importSettings;      // Importer'a özel ayarlar (örn. LOD sayısı, sıkıştırma kalitesi)
};

class AssetDatabase {
public:
    static AssetDatabase& instance() { static AssetDatabase db; return db; }

    AssetGuid getOrCreateGuid(const std::string& relativePath) {
        auto it = pathToGuid.find(relativePath);
        if (it != pathToGuid.end()) return it->second;

        AssetGuid newGuid = generateGuid();
        pathToGuid[relativePath] = newGuid;
        metadata[newGuid] = AssetMetadata{newGuid, relativePath, "", 0, {}};
        return newGuid;
    }

    const AssetMetadata* find(AssetGuid guid) const {
        auto it = metadata.find(guid);
        return it != metadata.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, AssetGuid> pathToGuid;
    std::unordered_map<AssetGuid, AssetMetadata> metadata;
};
```

### A.2 `.meta` dosyaları — GUID'lerin kalıcı olarak saklanması

`AssetDatabase` bellekte tutulan bir önbellek — ama GUID'ler her editör açılışında **aynı** kalmalı (yoksa Part'ların referansları her seferinde bozulur). Bu yüzden her import edilen dosyanın yanına, aynı isimde ama `.meta` uzantılı küçük bir dosya yazılır:

```
Assets/
├── character.fbx
├── character.fbx.meta      ← GUID + import ayarları burada, JSON olarak
├── wood_texture.png
└── wood_texture.png.meta
```

```cpp
// character.fbx.meta içeriği (örnek)
{
    "guid": "a3f8e21c-...",
    "importerType": "SkeletalMesh",
    "sourceFileHash": "8f3a...",
    "importSettings": { "generateLODs": true, "lodCount": 3, "importAnimations": true }
}
```

**Kritik pratik sonuç:** `.meta` dosyaları **Git'e commit edilmeli** (tıpkı Unity projelerinde olduğu gibi) — aksi halde bir takım arkadaşı projeyi klonladığında GUID'ler yeniden üretilir ve tüm asset referansları kopar. Bu, proje şablonuna (Faz 0) eklenmesi gereken bir `.gitattributes`/dokümantasyon notu.

---

## Bölüm B — Dosya Sistemi İzleme (File Watching)

### B.1 Neden gerekli

Kullanıcı, işletim sisteminin dosya gezgininden (Explorer/Finder) doğrudan proje klasörüne bir dosya sürükleyip bırakabilir, ya da harici bir programda (Blender, Photoshop) düzenlediği dosyayı kaydedebilir. Editörün bunu **otomatik olarak** fark edip yeniden import etmesi gerekiyor — kullanıcının her seferinde "Yeniden İçe Aktar" tuşuna basmasını beklemek kötü bir deneyim.

### B.2 Platform dosya izleme API'lerinin sarmalanması

```cpp
// Engine/Assets/FileWatcher.h

class FileWatcher {
public:
    using ChangeCallback = std::function<void(const std::string& path, FileChangeType type)>;

    void watch(const std::string& directory, ChangeCallback callback) {
        this->callback = callback;
#ifdef _WIN32
        watchHandle = FindFirstChangeNotificationA(directory.c_str(), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
#elif __APPLE__
        // FSEvents API kullanılır
#else
        // inotify (Linux) kullanılır
#endif
        watcherThread = std::thread([this]() { pollLoop(); });
    }

private:
    void pollLoop(); // Platforma özel bekleme + callback tetikleme döngüsü
    ChangeCallback callback;
    std::thread watcherThread;
};
```

**Not:** Platforma özel dosya izleme API'lerini (ReadDirectoryChangesW, FSEvents, inotify) sıfırdan sarmalamak yerine, bu iş için üretim kalitesinde hazır bir kütüphane (**efsw** — Entropia File System Watcher, açık kaynak, MIT lisanslı, üç platformu da destekliyor) kullanmak, "tekerleği yeniden icat etme" prensibimize daha uygun. Yukarıdaki kod kavramsal iskeleti gösteriyor; gerçek implementasyonda efsw'nin API'si sarmalanır.

### B.3 Değişiklik algılandığında ne oluyor

```cpp
void AssetImportPipeline::onFileChanged(const std::string& path, FileChangeType type) {
    if (type == FileChangeType::Deleted) {
        handleAssetDeletion(path);
        return;
    }

    // ★ .meta dosyalarındaki değişiklikleri yok say — sonsuz döngüye girmemek için
    //   (import işlemi kendisi .meta dosyasını günceller, bu da yeni bir "değişiklik" tetikler)
    if (path.ends_with(".meta")) return;

    uint64_t currentHash = computeFileHash(path);
    AssetGuid guid = AssetDatabase::instance().getOrCreateGuid(getRelativePath(path));
    AssetMetadata* meta = AssetDatabase::instance().findMutable(guid);

    if (meta->sourceFileHash == currentHash) return; // Dosya aslında değişmemiş (bazı editörler "dokunmadan kaydet" yapabiliyor)

    reimportAsset(guid, path); // ★ Bölüm C
}
```

**Dikkat edilmesi gereken tuzak:** `.meta` dosyalarını izlemezlik etmemek, kendi kendini tetikleyen bir geri besleme döngüsüne (feedback loop) yol açardı — import işlemi `.meta`'yı günceller, bu da "dosya değişti" bildirimini tetikler, bu da yeniden import'u tetikler, sonsuza dek. Bu, Faz 5'teki fizik senkronizasyon döngüsünde gördüğümüz "kaynağa geri yazmama" prensibinin dosya sistemi versiyonu.

---

## Bölüm C — Asenkron Import Pipeline

### C.1 Neden import işlemi ana thread'i bloklamamalı

Faz 3'teki Script Timeout dokümanında öğrendiğimiz dersin bir başka uygulaması burada karşımıza çıkıyor: Büyük bir FBX dosyasının import edilmesi (mesh işleme, LOD üretimi, animasyon çıkarma) saniyeler sürebilir. Bunu ana thread'de (editör UI thread'i) yaparsak, tüm editör o süre boyunca donar.

```cpp
// Engine/Assets/AssetImportPipeline.h

class AssetImportPipeline {
public:
    void reimportAsset(AssetGuid guid, const std::string& path) {
        // ★ Import işi, Faz 0'da kurulan job system'e (thread pool) devrediliyor
        JobSystem::instance().schedule([this, guid, path]() {
            ImportResult result = runImporterForFile(path); // Ağır iş burada, worker thread'de

            // Sonucu ana thread'e geri taşımak için bir "main thread queue" kullanılıyor
            MainThreadDispatcher::instance().enqueue([this, guid, result]() {
                applyImportResult(guid, result); // ★ DataModel/AssetDatabase güncellemesi SADECE ana thread'de
            });
        });
    }

private:
    void applyImportResult(AssetGuid guid, const ImportResult& result) {
        AssetDatabase::instance().updateMetadata(guid, result.metadata);
        AssetBrowserPanel::instance().refreshThumbnail(guid); // Bölüm D
        notifyDependentInstances(guid); // ★ Bölüm E — hot-reload
    }
};
```

**Bu tasarım kararı neden önemli:** `applyImportResult`'ın **sadece ana thread'de** çalışması bilinçli bir seçim — çünkü `AssetDatabase`, `DataModel` gibi paylaşılan veri yapıları thread-safe değil (Faz 5'teki fizik thread tartışmasında olduğu gibi). Worker thread sadece "saf hesaplama" yapıyor (dosya okuma, mesh işleme), hiçbir paylaşılan motor state'ine dokunmuyor; sonuç ana thread'e taşınıp orada uygulanıyor.

### C.2 İlerleme göstergesi (Progress Feedback)

Büyük bir asset klasörü ilk kez import edildiğinde (örn. bir proje yeni klonlandığında), kullanıcıya ilerleme göstermek önemli:

```cpp
struct ImportProgress {
    std::atomic<int> totalAssets{0};
    std::atomic<int> completedAssets{0};
};

// Editor/UI/ImportProgressOverlay.cpp
void ImportProgressOverlay::draw() {
    auto& progress = AssetImportPipeline::instance().getProgress();
    if (progress.completedAssets < progress.totalAssets) {
        ImGui::OpenPopup("Importing Assets");
        if (ImGui::BeginPopupModal("Importing Assets")) {
            float fraction = (float)progress.completedAssets / progress.totalAssets;
            ImGui::ProgressBar(fraction);
            ImGui::Text("%d / %d", progress.completedAssets.load(), progress.totalAssets.load());
            ImGui::EndPopup();
        }
    }
}
```

---

## Bölüm D — Thumbnail Üretimi

### D.1 Strateji: Gerçek render mi, ikon mu?

| Asset Türü | Thumbnail Stratejisi | Gerekçe |
|---|---|---|
| Texture (.png, .jpg) | Dosyanın kendisi küçültülüp gösterilir | Zaten görsel veri, ekstra işlem gereksiz |
| Mesh / Skeletal Mesh | **Gerçek render** — küçük bir framebuffer'a obje çizilir | Kullanıcı modeli önizlemeden tanıyamaz |
| Sound | Statik bir "hoparlör" ikonu + waveform önizlemesi | Ses görsel değil, waveform yeterli bilgi verir |
| Script | Statik bir "kod" ikonu | Kodun görsel bir karşılığı yok |

### D.2 Mesh thumbnail — Faz 2'nin Render-to-Texture'ının üçüncü kullanımı

Faz 4'te Viewport paneli için kullandığımız render-to-texture tekniği (Faz 4, Bölüm B.1), burada üçüncü kez faydasını gösteriyor:

```cpp
// Engine/Assets/ThumbnailRenderer.h

class ThumbnailRenderer {
public:
    bgfx::TextureHandle renderMeshThumbnail(MeshHandle mesh, MaterialHandle material) {
        static bgfx::FrameBuffer thumbnailFB = createThumbnailFramebuffer(128, 128); // Küçük, sabit boyut

        Camera thumbnailCamera = computeFramingCamera(mesh); // Mesh'in bounding box'ına göre otomatik kadraj

        RenderProxy proxy{Matrix4::identity(), mesh, material};
        Renderer::instance().renderSingleObject(thumbnailCamera, proxy, thumbnailFB, /*studioLighting=*/true);

        return getColorAttachment(thumbnailFB);
    }

private:
    Camera computeFramingCamera(MeshHandle mesh) {
        BoundingBox bounds = getMeshBounds(mesh);
        Vector3 center = bounds.center();
        float radius = bounds.diagonalLength() * 0.6f;

        Camera cam;
        cam.position = center + Vector3(1, 0.7f, 1).normalized() * radius; // 3/4 açı, tanıdık "ürün fotoğrafı" kadrajı
        cam.forward = (center - cam.position).normalized();
        return cam;
    }
};
```

`studioLighting=true` parametresi önemli — thumbnail'lar sahnenin gerçek ışıklandırmasından (Faz 7'deki GI, gölgeler vb.) bağımsız, sabit bir 3-nokta stüdyo ışıklandırmasıyla render edilmeli. Aksi halde bir asset'in thumbnail'ı, sahnede karanlık bir bölgede önizlendiğinde simsiyah görünürdü — bu, kullanıcı deneyimini bozardı.

### D.3 Thumbnail cache'leme — her karede yeniden render etmemek

```cpp
class ThumbnailCache {
public:
    bgfx::TextureHandle get(AssetGuid guid) {
        auto it = cache.find(guid);
        if (it != cache.end()) return it->second;

        // ★ Cache'de yoksa, bu karede render KUYRUĞUNA eklenir, hemen render edilmez
        pendingRenders.push_back(guid);
        return placeholderTexture; // Render tamamlanana kadar geçici bir "yükleniyor" ikonu gösterilir
    }

    void processPendingRenders(int maxPerFrame = 2) { // ★ Kritik: her karede en fazla 2 thumbnail render et
        for (int i = 0; i < maxPerFrame && !pendingRenders.empty(); i++) {
            AssetGuid guid = pendingRenders.back();
            pendingRenders.pop_back();
            cache[guid] = thumbnailRenderer.renderMeshThumbnail(/* ... */);
        }
    }

private:
    std::unordered_map<AssetGuid, bgfx::TextureHandle> cache;
    std::vector<AssetGuid> pendingRenders;
};
```

**Neden "her karede en fazla 2":** Kullanıcı, yüzlerce asset'in olduğu bir klasörü ilk kez açtığında, hepsini aynı anda render etmeye çalışmak o karede ciddi bir FPS düşüşüne (hitching) yol açar. Bunun yerine render işi karelere yayılıyor (throttling) — kullanıcı thumbnail'ların birkaç kare içinde "belirdiğini" görür, ama editör hiçbir zaman donmaz. Bu, Faz 3'teki Script Scheduler'ın "her karede sadece uyanması gerekenleri işle" felsefesinin bir başka tekrarı.

---

## Bölüm E — Hot-Reload: Sahnedeki Kullanımların Otomatik Güncellenmesi

### E.1 Sorun

Bir kullanıcı Blender'da bir karakterin mesh'ini düzenleyip kaydettiğinde, editördeki sahnede o mesh'i kullanan **tüm** Part'ların (belki onlarca) görsel olarak güncellenmesi gerekiyor — editörü yeniden başlatmadan.

### E.2 Bağımlılık takibi (Dependency Tracking)

```cpp
// Engine/Assets/AssetDependencyTracker.h

class AssetDependencyTracker {
public:
    // Bir Part, MeshHandle'ı sahneye eklerken kendini bu asset'in "kullanıcısı" olarak kaydeder
    void registerUsage(AssetGuid assetGuid, InstanceId userInstance) {
        usageMap[assetGuid].insert(userInstance);
    }

    void unregisterUsage(AssetGuid assetGuid, InstanceId userInstance) {
        usageMap[assetGuid].erase(userInstance);
    }

    const std::set<InstanceId>& getUsers(AssetGuid assetGuid) const {
        static std::set<InstanceId> empty;
        auto it = usageMap.find(assetGuid);
        return it != usageMap.end() ? it->second : empty;
    }

private:
    std::unordered_map<AssetGuid, std::set<InstanceId>> usageMap;
};
```

### E.3 Bölüm C.1'deki `notifyDependentInstances` fonksiyonunun gerçek implementasyonu

```cpp
void AssetImportPipeline::notifyDependentInstances(AssetGuid guid) {
    for (InstanceId userId : AssetDependencyTracker::instance().getUsers(guid)) {
        auto part = std::dynamic_pointer_cast<Part>(InstanceRegistry::instance().findById(userId));
        if (!part) continue;

        MeshHandle newMeshHandle = AssetDatabase::instance().resolveMeshHandle(guid);
        part->updateMeshHandle(newMeshHandle); // ★ Faz 2'deki RenderProxy'nin mesh alanı güncellenir

        // Eğer Faz 5'te bu mesh bir collision shape olarak da kullanılıyorsa, o da güncellenmeli
        if (part->usesmeshCollision()) part->rebuildPhysicsShape(newMeshHandle);
    }
}
```

**Bu zincirin önemi:** Tek bir asset güncellemesi, Faz 2 (RenderProxy), Faz 5 (collision shape) ve (eğer sahnede replicate edilen bir obje ise) Faz 6'daki (bir "asset güncellendi" bildirimi istemcilere de gitmesi gerekebilir — bu, ileri bir senaryo olarak not edilmeli) sistemlere aynı anda dokunuyor. `AssetDependencyTracker`, bu zincirin başlangıç noktası.

---

## Bölüm F — Asset Browser Paneli (ImGui Tarafı)

### F.1 Panel implementasyonu — Faz 4'ün desenlerinin tekrarı

```cpp
// Editor/UI/AssetBrowserPanel.h

class AssetBrowserPanel {
public:
    void draw() {
        ImGui::Begin("Asset Browser");

        drawFolderTree();  // Sol tarafta klasör ağacı (Explorer paneline benzer bir TreeNode yapısı)
        ImGui::SameLine();

        ImGui::BeginChild("AssetGrid");
        drawAssetGrid();   // Sağ tarafta thumbnail grid'i
        ImGui::EndChild();

        ImGui::End();
    }

private:
    void drawAssetGrid() {
        float cardSize = 96.0f;
        int columns = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cardSize));
        ImGui::Columns(columns, nullptr, false);

        for (auto& guid : getAssetsInCurrentFolder()) {
            bgfx::TextureHandle thumb = ThumbnailCache::instance().get(guid); // ★ Bölüm D.3
            ImGui::ImageButton((ImTextureID)(uintptr_t)thumb.idx, {cardSize, cardSize});

            // ★ Sürükle-bırak kaynağı — Bölüm F.2
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("ASSET_GUID", &guid, sizeof(AssetGuid));
                ImGui::Text("%s", AssetDatabase::instance().find(guid)->relativePath.c_str());
                ImGui::EndDragDropSource();
            }

            ImGui::Text("%s", getDisplayName(guid).c_str());
            ImGui::NextColumn();
        }
        ImGui::Columns(1);

        ThumbnailCache::instance().processPendingRenders(); // Her karede kuyruk işlenir
    }
};
```

### F.2 Sürükle-bırak — Viewport'a bırakınca sahneye ekleme

```cpp
// Editor/UI/Viewport.cpp — draw() fonksiyonunun sonuna eklenir

void ViewportPanel::handleAssetDrop() {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_GUID")) {
            AssetGuid droppedGuid = *(AssetGuid*)payload->Data;

            Vector3 worldPos = screenToWorldRay(ImGui::GetMousePos()).intersectWithGroundPlane();

            auto part = createInstance("Part"); // Faz 1'deki generic factory
            part->setMeshFromAsset(droppedGuid);  // AssetDependencyTracker'a otomatik kayıt olur (Bölüm E.2)
            part->position = worldPos;
            part->setParent(DataModel::instance().workspace);

            UndoStack::instance().pushCreateCommand(part, DataModel::instance().workspace); // Faz 4, Bölüm E
        }
        ImGui::EndDragDropTarget();
    }
}
```

**Bu, projenin tüm fazlarının bir araya geldiği somut bir kullanıcı anı:** Kullanıcı bir mesh dosyasını sürükleyip Viewport'a bırakıyor → AssetDatabase'den GUID çözülüyor → Faz 1'in generic factory'si ile Instance oluşturuluyor → Faz 2'nin RenderProxy'si otomatik kuruluyor → Faz 4'ün Undo sistemine kaydediliyor → (ileride Faz 5/6 devredeyse) fizik ve networking otomatik devreye giriyor. Hiçbir sistem bir diğerini "bilmeden" bu akış çalışıyor — her biri kendi sorumluluğunu, ortak noktalar (reflection, AssetDatabase, Signal) üzerinden yerine getiriyor.

---

## Bölüm G — "Definition of Done" Kontrol Listesi

- [ ] Proje klasörüne dışarıdan (Explorer/Finder ile) bırakılan bir dosya, editör açıkken otomatik algılanıp import ediliyor
- [ ] Her import edilen dosya için bir `.meta` dosyası oluşuyor, GUID editör yeniden başlatıldığında korunuyor
- [ ] `.meta` dosyalarındaki değişiklikler yeniden-import döngüsünü tetiklemiyor (feedback loop testi)
- [ ] Büyük bir dosyanın import'u sırasında editör UI'ı donmuyor (asenkron pipeline testi)
- [ ] Mesh/Skeletal Mesh thumbnail'ları gerçek render ile üretiliyor, sabit stüdyo ışıklandırmasıyla tutarlı görünüyor
- [ ] Yüzlerce asset'in olduğu bir klasör ilk açıldığında, thumbnail üretimi karelere yayılıyor (throttling), FPS düşüşü/donma olmuyor
- [ ] Bir mesh dosyası harici bir programda değiştirilip kaydedildiğinde, sahnedeki tüm kullanımları editörü yeniden başlatmadan güncelleniyor (hot-reload testi)
- [ ] Hot-reload, hem RenderProxy'yi hem (varsa) collision shape'i doğru güncelliyor
- [ ] Asset Browser'dan Viewport'a sürükle-bırak çalışıyor, doğru world-space pozisyonda obje oluşturuyor, Undo edilebiliyor
- [ ] Asset silindiğinde, o asset'i kullanan Instance'lara karşı anlamlı bir davranış sergileniyor (örn. placeholder/pembe materyal — sessizce çökmüyor)

---

## Sonraki Adım Önerisi

Asset Browser ile birlikte editörün temel iş akışı (obje oluştur → script yaz → asset import et → sahneye sürükle) artık uçtan uca tamamlanmış oldu. Kalan bekleyen konular:

1. **Katmanlı animasyon (Layered Animation):** Skeletal Animation dokümanının Bölüm B.3'ünde ertelenmişti.
2. **Interest Management derinleştirmesi:** Faz 6'dan beri bekliyor.
3. **Asset silme/taşıma senaryolarının derinleştirilmesi:** Bölüm G'deki son maddede bahsedilen "kırık referans" davranışının tam tasarımı — ayrı bir konu olarak ele alınabilir.

Hangisiyle devam edelim?
