# Interest Management — Teknik Derinlemesine İnceleme
## Faz 6'nın Basit Relevancy'sinden Gerçek Bir Replication Graph'a

Bu doküman, Faz 6, Bölüm C.4'te bilinçli olarak basit bırakılan mesafe-tabanlı `isRelevant()` fonksiyonunu, yüzlerce eş zamanlı oyuncuyu ve binlerce Part'ı destekleyebilecek gerçek bir sisteme dönüştürmeyi inceler. Hedef: Karakter Kontrolcüsü dokümanının Bölüm D.2'sinde tespit edilen "hareketli platform" sorununu da kalıcı olarak çözmek.

---

## Bölüm A — Faz 6'nın Basit Modelinin Nerede Kırıldığı

### A.1 Mevcut sistemin sınırları

Faz 6'daki `isRelevant()` fonksiyonu, her (istemci, obje) çifti için her karede bir mesafe hesabı yapıyordu:

```cpp
// Faz 6, Bölüm C.4 — hatırlatma
bool isRelevant(const std::shared_ptr<Instance>& inst) const {
    float distance = (part->position - playerCharacterPosition).length();
    return distance < RELEVANCY_RADIUS;
}
```

Bu yaklaşımın karmaşıklığı **O(oyuncu sayısı × obje sayısı)** — 100 oyuncu ve 10.000 Part'lık bir sahnede, her karede 1.000.000 mesafe hesabı demek. 20Hz replikasyon hızında bu, saniyede 20 milyon hesaplama — tek başına bu hesap CPU'yu ciddi şekilde yorabilir, daha replikasyon paketlerini oluşturmaya bile gelmeden.

### A.2 İkinci sorun: Ani görünürlük değişimleri (flickering)

Bir oyuncu, relevancy sınırının (`RELEVANCY_RADIUS`) tam kenarında ileri geri hareket ettiğinde (ya da başka bir oyuncu/obje hareket ettiğinde), bir obje sürekli "relevant / not relevant" arasında gidip gelebilir — bu da o objenin istemcide sürekli yaratılıp yok edilmesine (`createInstanceFromReplication` / obje silme) yol açar, hem gereksiz bant genişliği hem de görsel "titreme" (pop-in/pop-out) yaratır.

### A.3 Üçüncü sorun: "Her zaman önemli" kategorisi yok

Bazı objeler mesafeden bağımsız her zaman replicate edilmeli — örneğin bir oyun modundaki skor tablosu, ya da (Karakter Kontrolcüsü dokümanında tespit edilen) bir oyuncunun üzerinde durduğu hareketli platform. Faz 6'nın basit modeli bu tür istisnaları desteklemiyordu.

---

## Bölüm B — Çözüm 1: Spatial Grid ile O(N×M)'den O(N)'e İnme

### B.1 Temel fikir

Sahneyi sabit boyutlu hücrelere (örn. 100x100 stud) bölen bir ızgara (grid) tutuyoruz. Her Part, hangi hücrede olduğunu biliyor; her oyuncu da hangi hücrelerin "görüş menzilinde" olduğunu biliyor. Mesafe hesabı yerine, **hücre üyeliği** kontrol ediliyor — bu, bir hash-map lookup'ı kadar ucuz.

```cpp
// Engine/Networking/Replication/SpatialGrid.h

struct GridCoord {
    int x, z;
    bool operator==(const GridCoord& o) const { return x == o.x && z == o.z; }
};

struct GridCoordHash {
    size_t operator()(const GridCoord& c) const { return std::hash<int64_t>()(((int64_t)c.x << 32) | (uint32_t)c.z); }
};

class SpatialGrid {
public:
    static constexpr float CELL_SIZE = 100.0f; // stud

    GridCoord worldToGrid(const Vector3& pos) const {
        return { (int)std::floor(pos.x / CELL_SIZE), (int)std::floor(pos.z / CELL_SIZE) };
    }

    void updateInstancePosition(InstanceId id, const Vector3& newPos) {
        GridCoord newCoord = worldToGrid(newPos);
        GridCoord oldCoord = instanceToCoord[id];

        if (newCoord == oldCoord) return; // ★ Hücre değişmediyse hiçbir işlem yapma — çoğu obje çoğu karede burada durur

        cells[oldCoord].erase(id);
        cells[newCoord].insert(id);
        instanceToCoord[id] = newCoord;
    }

    // Bir merkez etrafındaki N hücre yarıçapındaki tüm objeleri döndürür
    std::vector<InstanceId> queryRadius(const Vector3& center, int cellRadius) const {
        std::vector<InstanceId> result;
        GridCoord centerCoord = worldToGrid(center);

        for (int dx = -cellRadius; dx <= cellRadius; dx++) {
            for (int dz = -cellRadius; dz <= cellRadius; dz++) {
                auto it = cells.find({centerCoord.x + dx, centerCoord.z + dz});
                if (it != cells.end())
                    result.insert(result.end(), it->second.begin(), it->second.end());
            }
        }
        return result;
    }

private:
    std::unordered_map<GridCoord, std::unordered_set<InstanceId>, GridCoordHash> cells;
    std::unordered_map<InstanceId, GridCoord> instanceToCoord;
};
```

**Bu tasarımın performans kazancı nereden geliyor:** `updateInstancePosition`, bir Part her pozisyon değiştirdiğinde (yani `markPropertyDirty` her tetiklendiğinde — Faz 6, Bölüm C.2 ile aynı tetikleyici noktadan besleniyor) çağrılıyor, ama **sadece hücre değiştiğinde** gerçek bir güncelleme yapıyor. Bir Part aynı 100x100 stud'lık alan içinde hareket ettiği sürece (ki çoğu obje için bu böyledir), bu fonksiyon anında geri dönüyor. `queryRadius` ise, oyuncu sayısından bağımsız olarak sadece **ilgili hücrelerdeki** objeleri tarıyor — mesafe hesabı artık tüm sahne yerine küçük bir alt kümede yapılıyor.

### B.2 ReplicationManager'a entegrasyon

```cpp
// Faz 6, Bölüm C.2'deki flushToAllClients fonksiyonunun güncellenmesi

void ReplicationManager::flushToAllClients(float deltaTime) {
    // ... zamanlayıcı kontrolü aynı ...

    for (auto& client : connectedClients) {
        // ★ Artık her Part için tek tek mesafe hesaplamak yerine, doğrudan yakın hücrelerdeki objeler sorgulanıyor
        std::vector<InstanceId> nearbyInstances = spatialGrid.queryRadius(
            client.playerCharacterPosition, /*cellRadius=*/3 // 3 hücre ≈ 300 stud görüş menzili
        );

        for (InstanceId id : nearbyInstances) {
            if (!dirtyProperties.contains(id)) continue; // Sadece bu karede değişmiş olanlar
            sendPositionUpdate(client.connection, buildPacket(id, dirtyProperties[id]));
        }
    }
}
```

---

## Bölüm C — Çözüm 2: Hysteresis ile Titremeyi Önleme

### C.1 İki farklı yarıçap kullanmak

Titreme sorununu çözmenin standart yöntemi, "görünür olma" ve "görünmez olma" için **farklı** eşikler kullanmak — buna hysteresis (histerezis) deniyor:

```cpp
// Engine/Networking/Replication/RelevancyTracker.h

class RelevancyTracker {
public:
    static constexpr float ENTER_RADIUS = 300.0f;  // Bu mesafenin altına girince "relevant" olur
    static constexpr float EXIT_RADIUS = 400.0f;   // Bu mesafenin ÜSTÜNE çıkınca "not relevant" olur (★ daha büyük)

    void update(ClientConnection& client, InstanceId id, float distance) {
        bool currentlyRelevant = client.relevantInstances.contains(id);

        if (!currentlyRelevant && distance < ENTER_RADIUS) {
            client.relevantInstances.insert(id);
            sendFullReplicationPacket(client, id); // Obje ilk kez görünür oluyor — tüm property'ler gönderilir
        }
        else if (currentlyRelevant && distance > EXIT_RADIUS) {
            client.relevantInstances.erase(id);
            sendDestroyInstancePacket(client, id); // Obje istemcide siliniyor
        }
        // ★ 300-400 stud arasındaki "tampon bölge"de HİÇBİR ŞEY DEĞİŞMİYOR — obje ne yeni ekleniyor ne siliniyor
    }
};
```

**Bu neden titremeyi çözüyor:** Bir oyuncu tam olarak 300 stud sınırında ileri geri hareket etse bile, sistem onu 400 stud'a çıkana kadar "relevant" tutmaya devam ediyor — 300-400 arasındaki 100 stud'lık tampon bölge, ufak hareket dalgalanmalarının sürekli obje oluşturma/silme tetiklememesini sağlıyor.

---

## Bölüm D — Çözüm 3: Öncelik Tabanlı Bant Genişliği Dağıtımı

### D.1 Sorun: Görüş menzilindeki HER şeyi aynı sıklıkta göndermek gerekmiyor

Bir oyuncunun görüş menzilinde 200 Part olabilir ama bunların hepsi eşit derecede "önemli" değil — oyuncuya çok yakın, hızlı hareket eden bir obje (örn. az önce fırlatılan bir top) yüksek sıklıkta güncellenmeli; uzakta, yavaş hareket eden bir obje daha seyrek güncellenebilir.

```cpp
// Engine/Networking/Replication/PriorityCalculator.h

float calculateReplicationPriority(const ClientConnection& client, const RenderProxy& proxy, InstanceId id) {
    float distance = (proxy.worldTransform.getTranslation() - client.playerCharacterPosition).length();
    float distanceFactor = 1.0f - std::clamp(distance / RelevancyTracker::EXIT_RADIUS, 0.0f, 1.0f);

    float velocityFactor = std::min(getInstanceVelocity(id).length() / 50.0f, 1.0f); // Hızlı hareket eden objeler öncelikli

    float timeSinceLastSentFactor = getTimeSinceLastSent(client, id) / MAX_STALE_TIME; // ★ Uzun süredir gönderilmemiş bir obje öncelik kazanır (starvation'ı önler)

    return distanceFactor * 0.5f + velocityFactor * 0.3f + timeSinceLastSentFactor * 0.2f;
}
```

### D.2 Her karede sabit bir "paket bütçesi" içinde en yüksek öncelikli olanları seçmek

```cpp
void ReplicationManager::flushToAllClients(float deltaTime) {
    for (auto& client : connectedClients) {
        std::vector<InstanceId> nearby = spatialGrid.queryRadius(client.playerCharacterPosition, 3);

        // Her obje için öncelik hesapla, en yükseklerden başlayarak sırala
        std::vector<std::pair<InstanceId, float>> prioritized;
        for (InstanceId id : nearby)
            prioritized.push_back({id, calculateReplicationPriority(client, getProxy(id), id)});

        std::partial_sort(prioritized.begin(),
            prioritized.begin() + std::min((size_t)MAX_UPDATES_PER_CLIENT_PER_TICK, prioritized.size()),
            prioritized.end(), [](auto& a, auto& b) { return a.second > b.second; });

        // ★ Sadece bütçe kadar (örn. en fazla 64 obje) bu karede gönderilir, geri kalanı sonraki kareye kalır
        for (int i = 0; i < std::min((int)MAX_UPDATES_PER_CLIENT_PER_TICK, (int)prioritized.size()); i++) {
            sendPositionUpdate(client.connection, buildPacket(prioritized[i].first));
            markAsSent(client, prioritized[i].first);
        }
    }
}
```

**Bu tasarımın kritik faydası:** Sunucunun her istemciye gönderdiği paket sayısı artık **sınırlı ve öngörülebilir** (`MAX_UPDATES_PER_CLIENT_PER_TICK`) — kalabalık bir bölgede (örn. 500 oyuncunun toplandığı bir etkinlik alanı) bile sunucu, kontrolsüz şekilde bant genişliğini tüketmiyor. `timeSinceLastSentFactor` faktörü de "starvation" (bir objenin sürekli düşük öncelikli kalıp hiç güncellenmemesi) riskini önlüyor — uzun süre gönderilmeyen bir obje zamanla önceliğini artırıyor.

---

## Bölüm E — "Her Zaman İlgili" Kategorisi ve Karakter Kontrolcüsü Sorununun Çözümü

### E.1 AlwaysRelevant işaretleyicisi

```cpp
struct PropertyDescriptor {
    // ... önceki alanlar ...
};

// Instance seviyesinde bir flag — Faz 1.5'teki category/readOnly desenine benzer bir genişleme
class Instance {
public:
    bool alwaysRelevant = false; // ★ Yeni
};
```

```cpp
void ReplicationManager::flushToAllClients(float deltaTime) {
    for (auto& client : connectedClients) {
        // 1. Her zaman ilgili objeler — spatial grid sorgusundan BAĞIMSIZ olarak her zaman dahil edilir
        for (InstanceId id : AlwaysRelevantRegistry::instance().getAll())
            sendPositionUpdate(client.connection, buildPacket(id));

        // 2. Normal mesafe/öncelik tabanlı objeler (Bölüm D)
        // ...
    }
}
```

### E.2 Karakter Kontrolcüsü dokümanının Bölüm D.2'sindeki sorunun kalıcı çözümü

O dokümanda şöyle bir tespit yapılmıştı: *"Hareketli platformlar gibi karakterin hareketini etkileyebilecek objeler, relevancy filtresinden bağımsız olarak her zaman yüksek öncelikli replicate edilmeli."* Artık bunun somut mekanizması var:

```cpp
void MovingPlatform::onAddedToWorkspace() {
    // ... Faz 2/5'teki normal proxy/body kurulumu ...
    alwaysRelevant = true; // ★ Bu tek satır, artık Bölüm E.1'deki mekanizmayı devreye sokuyor
}
```

**Neden bu, dağınık bir özel durum değil, sistemin doğal bir parçası:** `AlwaysRelevantRegistry`, aynı `Instance` temel sınıfının bir alanına bakıyor — herhangi bir gelecekteki obje türü (örn. bir "hedef bayrağı" ya da "boss canavarı") de aynı mekanizmayı, hiçbir networking kodu yazmadan, sadece `alwaysRelevant = true` diyerek kullanabiliyor.

---

## Bölüm F — Dormancy: Hareket Etmeyen Objelerin Ekstra Optimizasyonu

### F.1 Fikir

Faz 5'teki Jolt entegrasyonunda `GetActiveBodies()` ile sadece hareket eden body'leri sorguluyorduk (uykudaki/durgun body'ler atlanıyordu). Aynı prensip, replication tarafında da uygulanabilir: Uzun süre hiç değişmemiş (dirty olmamış) bir obje, "dormant" (uykuda) olarak işaretlenip spatial grid sorgularından bile çıkarılabilir — tekrar hareket ettiğinde (bir Jolt aktivasyonu veya script müdahalesiyle) otomatik olarak "uyandırılır."

```cpp
class DormancyManager {
public:
    void onInstanceDirty(InstanceId id) {
        dormantInstances.erase(id); // Değişiklik oldu — uykudan çık
        lastDirtyTime[id] = currentServerTime;
    }

    void updateDormancy(float currentTime) {
        for (auto& [id, lastDirty] : lastDirtyTime) {
            if (currentTime - lastDirty > DORMANCY_THRESHOLD && !dormantInstances.contains(id)) {
                dormantInstances.insert(id);
                spatialGrid.removeFromActiveQueries(id); // ★ Artık queryRadius sonuçlarında görünmeyecek
            }
        }
    }

private:
    static constexpr float DORMANCY_THRESHOLD = 30.0f; // 30 saniye hareketsiz kalınca dormant
    std::unordered_set<InstanceId> dormantInstances;
    std::unordered_map<InstanceId, float> lastDirtyTime;
};
```

**Bu neden güvenli:** Dormant bir obje, istemcide **zaten** son bilinen (doğru) konumunda duruyor — bir daha güncelleme göndermemek görsel bir soruna yol açmıyor, çünkü zaten değişen bir şey yok. Obje tekrar hareket ettiğinde `onInstanceDirty` tetiklenip anında aktif hale dönüyor.

---

## Bölüm G — Yeni Bağlanan İstemci: İlk Senkronizasyon (Initial Sync)

### G.1 Bir sorun: Oyuncu ilk katıldığında etrafındaki 200 objeyi tek karede mi göndermeli?

Bir oyuncu sunucuya yeni bağlandığında, `RelevancyTracker` onun etrafındaki tüm objeleri "yeni relevant" olarak işaretler (Bölüm C.1) — eğer bunların hepsi tek bir pakette gönderilirse, o istemci için ani bir bant genişliği patlaması (spike) olur, hatta GNS paket boyutu sınırlarını aşabilir.

```cpp
void ClientConnection::onPlayerJoined() {
    std::vector<InstanceId> initialSet = spatialGrid.queryRadius(spawnPosition, 3);
    pendingInitialSync.assign(initialSet.begin(), initialSet.end()); // ★ Kuyruğa alınır, hemen gönderilmez
}

void ReplicationManager::flushToAllClients(float deltaTime) {
    for (auto& client : connectedClients) {
        // Her karede initial sync kuyruğundan sınırlı sayıda obje gönder (Bölüm D'deki throttling ile aynı felsefe)
        int sent = 0;
        while (!client.pendingInitialSync.empty() && sent < INITIAL_SYNC_BATCH_SIZE) {
            sendFullReplicationPacket(client, client.pendingInitialSync.back());
            client.pendingInitialSync.pop_back();
            sent++;
        }
        // ... normal öncelik tabanlı akış (Bölüm D) initial sync tamamlandıktan sonra devam eder
    }
}
```

Bu, Asset Browser dokümanındaki thumbnail üretim throttling'i ile (Bölüm D.3, "her karede en fazla 2 thumbnail") **birebir aynı desenin** networking tarafındaki karşılığı — ani yük her zaman karelere yayılarak yumuşatılıyor.

---

## Bölüm H — "Definition of Done" Kontrol Listesi

- [ ] `SpatialGrid` doğru çalışıyor — bir Part hücre sınırını geçtiğinde doğru şekilde eski hücreden çıkarılıp yeni hücreye ekleniyor
- [ ] 10.000+ Part'lık bir sahnede, `queryRadius` çağrısının süresi Faz 6'nın ham mesafe taramasına kıyasla ölçülebilir şekilde daha hızlı (profiling ile karşılaştırma)
- [ ] Hysteresis çalışıyor — bir oyuncu relevancy sınırında ileri geri hareket ettiğinde obje sürekli oluşturulup silinmiyor (görsel titreme testi)
- [ ] Öncelik hesaplaması doğru çalışıyor — yakın/hızlı objeler, uzak/durgun objelere göre daha sık güncelleniyor
- [ ] `MAX_UPDATES_PER_CLIENT_PER_TICK` bütçesi aşılmıyor — kalabalık bir sahnede paket sayısı sınırlı kalıyor
- [ ] `timeSinceLastSentFactor` sayesinde hiçbir obje süresiz olarak "aç" (starved) kalmıyor — uzun süre gönderilmeyen objeler zamanla önceliğini artırıyor
- [ ] `alwaysRelevant = true` işaretli bir obje (örn. hareketli platform), mesafeden bağımsız her zaman replicate ediliyor
- [ ] Karakter Kontrolcüsü'ndeki hareketli platform senaryosu artık prediction sapması yaşamıyor (regresyon testi)
- [ ] Dormancy sistemi çalışıyor — uzun süre hareketsiz kalan objeler `queryRadius` sonuçlarından düşüyor, tekrar hareket ettiğinde otomatik geri dönüyor
- [ ] Yeni bağlanan bir istemcide initial sync, tek bir pakette değil, karelere yayılmış şekilde gerçekleşiyor — bağlanma anında bant genişliği spike'ı oluşmuyor

---

## Sonraki Adım Önerisi

Interest Management derinleştirmesiyle birlikte, en başından beri açık kalan **tüm** dallanma noktaları artık kapatılmış oldu:

- Faz 0-8 (temel yol haritası)
- Faz 1 derinleştirme (kalıtım + karmaşık tipler)
- Script Timeout
- Karakter Kontrolcüsü (Humanoid)
- Skeletal Animation
- Asset Browser / Import Akışı
- Katmanlı Animasyon
- Interest Management (bu doküman)

Toplamda 13 doküman, projenin mimarisini uçtan uca kapsıyor. Buradan üç şekilde ilerleyebiliriz:

1. **Genel bir "Master Index" dokümanı:** Tüm 13 dokümanı, aralarındaki bağımlılıkları ve önerilen gerçek geliştirme sırasını (hangi dokümanın hangi dokümanı önkoşul olarak gerektirdiği) tek bir haritada özetleyen bir referans doküman.
2. **Gerçek koda başlama:** Faz 0'daki CMake iskeletini gerçekten kurmaya başlamak.
3. **Şimdiye kadar hiç değinmediğimiz bir konuyu açmak** (örn. Ses sistemi, Terrain/arazi sistemi, Lokalizasyon, Plugin/Marketplace ekosistemi gibi).

Hangisini istersin?
