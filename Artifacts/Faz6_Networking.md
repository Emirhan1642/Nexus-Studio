# Faz 6 — Teknik Derinlemesine İnceleme
## Networking: Sunucu-Otoriteli Multiplayer Mimarisi

Bu doküman, Faz 1-5'te kurulan tek-oyunculu motoru, Roblox tarzı sunucu-otoriteli (server-authoritative) çoklu oyunculu bir mimariye dönüştürmeyi inceler. Hedef: Sunucudaki bir `Part`'ın pozisyon değişikliğinin otomatik olarak istemcilere yansıması, ve script'ten sunucu-istemci mesajlaşmasının (`RemoteEvent`) çalışması.

---

## Bölüm A — Temel Mimari Karar: Sunucu-Otoriteli Model

### A.1 Neden bu model seçildi (alternatiflere karşı)

| Model | Açıklama | Hile Direnci | Roblox'a Uygunluk |
|---|---|---|---|
| Peer-to-Peer | Her istemci diğerleriyle doğrudan konuşur | Çok düşük — herkes herkesin verisini manipüle edebilir | ❌ |
| İstemci-otoriteli | Her istemci kendi state'inin sahibi, sunucu sadece relay yapar | Düşük — hile yazılımı kolayca "ben 1000 hasar verdim" diyebilir | ❌ |
| **Sunucu-otoriteli** | Sunucu "gerçeği" belirler, istemciler sadece görüntüler + input gönderir | Yüksek | ✅ **Roblox'un kullandığı model** |

**Karar: Tam sunucu-otoriteli mimari.** Bunun somut anlamı: `DataModel` sahnesinin **gerçek/orijinal kopyası sadece sunucuda yaşar**. İstemcideki DataModel, sunucudaki DataModel'in bir "yansımasıdır" (replica) — istemci bir Part'ın pozisyonunu doğrudan değiştiremez, sadece sunucudan gelen güncellemeleri render eder.

### A.2 Bunun motor mimarisine somut etkisi

Faz 0-5'te inşa ettiğimiz her şey (`DataModel`, `Instance`, `Script`) hem sunucu hem istemci tarafında **aynı kod** olarak çalışacak — ama her `Instance`'ın bir `NetworkOwnership` bilgisi olacak (sunucu mu, yerel mi). Bu, ayrı bir "sunucu motoru" ve "istemci motoru" yazmak yerine **tek bir executable'ın iki farklı modda çalışması** anlamına geliyor:

```cpp
enum class NetworkMode { Standalone, Server, Client };
```

`Standalone` modu, Faz 0-5'te test ettiğimiz tek oyunculu deneyimin ta kendisi — networking hiç devrede değilken kullanılan mod. Bu tasarım kararı önemli: **networking, mevcut sistemleri bozan bir ek katman değil, üzerine eklenen isteğe bağlı bir mod.**

---

## Bölüm B — Transport Katmanı: GameNetworkingSockets (GNS)

### B.1 Neden ham UDP/TCP değil, GNS?

TCP, paket kaybında tüm sonraki paketleri bekletir (head-of-line blocking) — bir pozisyon güncellemesi gecikirse sonraki tüm mesajlar da gecikir, bu gerçek zamanlı hareket için kabul edilemez. Ham UDP ise güvenilirlik (paket ulaştı mı?), sıralama, şifreleme gibi her şeyi sıfırdan yazmayı gerektirir.

**GNS (Valve, açık kaynak)** bize şunu veriyor: UDP temelli ama **kanal başına seçilebilir güvenilirlik** (bazı mesajlar "mutlaka ulaşsın" — örn. bir obje oluşturma; bazıları "en yenisi yeterli, eskisi boşver" — örn. pozisyon güncellemesi), yerleşik şifreleme, ve bağlantı yönetimi.

### B.2 Bağlantı kurulumu (sunucu tarafı)

```cpp
// Engine/Networking/Transport/NetworkServer.h

#pragma once
#include <steam/isteamnetworkingsockets.h>

class NetworkServer {
public:
    void startListening(uint16_t port) {
        SteamNetworkingIPAddr localAddr{};
        localAddr.Clear();
        localAddr.m_port = port;

        SteamNetworkingConfigValue_t opt;
        opt.SetPtr(k_ESteamNetworkingConfig_Callback_ConnectionStatusChanged,
                   (void*)onConnectionStatusChangedStatic);

        listenSocket = SteamNetworkingSockets()->CreateListenSocketIP(localAddr, 1, &opt);
    }

    void pollIncomingMessages() {
        ISteamNetworkingMessage* messages[32];
        int count = SteamNetworkingSockets()->ReceiveMessagesOnListenSocket(listenSocket, messages, 32);

        for (int i = 0; i < count; i++) {
            handleIncomingPacket(messages[i]->m_conn, messages[i]->m_pData, messages[i]->m_cbSize);
            messages[i]->Release();
        }
    }

private:
    HSteamListenSocket listenSocket;
    static void onConnectionStatusChangedStatic(SteamNetConnectionStatusChangedCallback_t* info);
};
```

### B.3 Kanal stratejisi

```cpp
enum class NetChannel : int {
    Reliable_Ordered   = 0, // Obje oluşturma/silme, RemoteEvent çağrıları — mutlaka ve sırayla ulaşmalı
    Unreliable_State   = 1, // Pozisyon/rotasyon güncellemeleri — en yenisi yeterli
};

void sendPositionUpdate(HSteamNetConnection conn, const ReplicationPacket& packet) {
    SteamNetworkingSockets()->SendMessageToConnection(
        conn, packet.data(), packet.size(),
        k_nSteamNetworkingSend_Unreliable, // ★ Eski pozisyon paketi kaybolsa da önemli değil, yenisi zaten geliyor
        nullptr
    );
}

void sendRemoteEvent(HSteamNetConnection conn, const RemoteEventPacket& packet) {
    SteamNetworkingSockets()->SendMessageToConnection(
        conn, packet.data(), packet.size(),
        k_nSteamNetworkingSend_Reliable, // ★ Bir RemoteEvent kaybolursa oyun mantığı bozulur, mutlaka ulaşmalı
        nullptr
    );
}
```

Bu ayrım kritik bir performans/güvenilirlik dengesi: Pozisyon güncellemeleri saniyede 20-30 kez gönderiliyor, birinin kaybolması önemsiz (bir sonraki zaten güncel veriyi taşıyor). Ama bir "objeyi yok et" komutu kaybolursa istemci hayalet bir obje görmeye devam eder — bu yüzden güvenilir kanal şart.

---

## Bölüm C — DataModel Replication: Reflection'ın Dördüncü Kullanımı

### C.1 Hangi property'ler replicate edilir?

Faz 1.5'te `PropertyDescriptor`'a eklediğimiz metadata alanlarına (`category`, `readOnly`) bir tane daha ekliyoruz:

```cpp
struct PropertyDescriptor {
    // ... önceki alanlar (Faz 1, Faz 1.5) ...
    bool replicated = true; // ★ Yeni — varsayılan olarak her property replicate edilir
};

// ClassBuilder'a küçük bir zincirleme eklentisi
ClassBuilder& noReplicate() {
    if (!descriptor->properties.empty())
        descriptor->properties.back().replicated = false;
    return *this;
}
```

Kullanım — örneğin bir Part'ın sadece sunucuda anlamlı olan, istemciye gitmesi gereksiz bir debug alanı varsa:

```cpp
ClassBuilder<Part>("Part")
    .property("Position", &Part::position)             // replicate edilir (varsayılan)
    .property("ServerDebugId", &Part::debugId).noReplicate(); // istemciye hiç gönderilmez
```

### C.2 Replication akışı — sunucu tarafı

Faz 2'de kurduğumuz "dirty" deseni burada da tekrarlanıyor — bu sefer dirty olan şey render değil, **ağ üzerinden gönderilmesi gereken property**:

```cpp
// Engine/Networking/Replication/ReplicationManager.h

class ReplicationManager {
public:
    // Faz 1'deki PropertyDescriptor::setter her çağrıldığında bu tetiklenir
    void markPropertyDirty(InstanceId id, const std::string& propName) {
        dirtyProperties[id].insert(propName);
    }

    void flushToAllClients(float deltaTime) {
        replicationTimer += deltaTime;
        if (replicationTimer < REPLICATION_INTERVAL) return; // ★ Her karede değil, saniyede ~20 kez
        replicationTimer = 0.0f;

        for (auto& [instanceId, dirtyProps] : dirtyProperties) {
            auto inst = InstanceRegistry::instance().findById(instanceId);
            if (!inst) continue;

            ReplicationPacket packet = buildPacket(inst, dirtyProps);
            for (auto& client : connectedClients)
                if (client.isRelevant(inst)) // ★ Bölüm C.4 — herkese her şey gönderilmez
                    sendPositionUpdate(client.connection, packet);
        }
        dirtyProperties.clear();
    }

private:
    ReplicationPacket buildPacket(const std::shared_ptr<Instance>& inst,
                                    const std::set<std::string>& dirtyProps) {
        auto* classDesc = TypeRegistry::instance().find(getClassName(inst));
        ReplicationPacket packet;
        packet.instanceId = inst->getInstanceId();

        for (auto& propName : dirtyProps) {
            auto* prop = classDesc->findProperty(propName);
            if (!prop->replicated) continue; // ★ noReplicate() işaretli alanlar asla paket'e girmez
            packet.writeProperty(propName, prop->getter(inst.get()));
        }
        return packet;
    }

    std::unordered_map<InstanceId, std::set<std::string>> dirtyProperties;
    float replicationTimer = 0.0f;
    static constexpr float REPLICATION_INTERVAL = 1.0f / 20.0f; // 20 Hz gönderim sıklığı
};
```

**Bu tasarımın önemi:** `Part::setPosition()` fonksiyonu artık (Faz 2'de RenderProxy, Faz 5'te Jolt body'e ek olarak) **dördüncü** bir sistemi daha tetikliyor: `ReplicationManager::markPropertyDirty()`. Reflection setter'ı, projedeki en çok "abone"si olan merkezi nokta haline geldi — bu, Faz 1'de bu API'yi özenle tasarlamanın getirisi.

### C.3 İstemci tarafı — paket alma ve DataModel'e uygulama

```cpp
void NetworkClient::handleReplicationPacket(const ReplicationPacket& packet) {
    auto inst = InstanceRegistry::instance().findById(packet.instanceId);
    if (!inst) {
        // Bu instance istemcide hiç yok — sunucu "yeni obje" bildirimi göndermiş olmalı
        inst = createInstanceFromReplication(packet); // Reflection'ın generic factory'si (Faz 1) burada da devrede
    }

    auto* classDesc = TypeRegistry::instance().find(getClassName(inst));
    for (auto& [propName, value] : packet.properties) {
        auto* prop = classDesc->findProperty(propName);
        prop->setter(inst.get(), value); // ★ Aynı reflection setter'ı — DataModel + RenderProxy otomatik güncellenir
    }
}
```

**Kritik nokta:** İstemci tarafında `prop->setter()` çağrıldığında, Faz 2'deki `markRenderDirty()` da otomatik tetikleniyor (çünkü aynı `Part::setPosition()` fonksiyonu çalışıyor) — yani networking kodu render'ı hiç bilmiyor, sadece reflection'a yazıyor, geri kalan zincir kendiliğinden işliyor. **Ancak dikkat:** istemci tarafında Jolt fizik senkronizasyonunun (Faz 5, Bölüm C.2) tekrar tetiklenmemesi gerekir — istemci fiziği sadece görsel yorumlama için kullanmalı, bu Bölüm D'de ele alınıyor.

### C.4 Relevancy (İlgililik) — neden herkese her şey gönderilmiyor?

Roblox tarzı büyük bir sahnede binlerce Part olabilir; her istemciye her Part'ın her değişikliğini göndermek bant genişliğini tüketir. Basit bir relevancy stratejisi (Faz 6 MVP için yeterli):

```cpp
bool ClientConnection::isRelevant(const std::shared_ptr<Instance>& inst) const {
    auto part = std::dynamic_pointer_cast<Part>(inst);
    if (!part) return true; // Part olmayan (Script, Folder vb.) her zaman replicate edilir

    float distance = (part->position - playerCharacterPosition).length();
    return distance < RELEVANCY_RADIUS; // örn. 500 stud
}
```

**Not:** Bu, Unreal'ın çok daha gelişmiş "Replication Graph" sisteminin basitleştirilmiş bir versiyonu. MVP için mesafe-tabanlı filtreleme yeterli; ileride (ölçeklenirken) daha akıllı bir ilgi alanı (interest management) sistemi gerekebilir — bu, ayrı bir derinleştirme konusu olarak not edilmeli.

---

## Bölüm D — Client-Side Prediction

### D.1 Sorun: Ham senkronizasyon neden yetersiz?

Eğer istemci hiçbir tahmin yapmadan sadece sunucudan gelen pozisyonları render etseydi: oyuncu bir tuşa bastığında, komut sunucuya gidip (ping süresi kadar gecikme), sunucu işleyip, sonuç geri gelene kadar (bir ping süresi daha) karakter **hareket etmiyormuş gibi** görünürdü. 100ms ping'te bu, gözle görülür, oynanamaz bir gecikme demektir.

**Çözüm:** İstemci, kendi karakterinin hareketini **hemen, tahminî olarak** uygular (sunucuyu beklemeden), sonra sunucudan "gerçek" sonuç geldiğinde ufak bir düzeltme (reconciliation) yapar.

### D.2 Input tabanlı prediction akışı

```cpp
// Engine/Networking/Prediction/ClientPredictor.h

struct InputCommand {
    uint32_t sequenceNumber; // Her input'a artan bir numara veriliyor
    Vector3 moveDirection;
    bool jumpPressed;
    float deltaTime;
};

class ClientPredictor {
public:
    void onLocalInput(const InputCommand& cmd) {
        pendingCommands.push_back(cmd);

        // ★ İstemci, sunucuyu beklemeden KENDİ karakterine hemen uygular
        applyMovementLocally(localCharacter, cmd);

        sendToServer(cmd); // Aynı komut sunucuya da gönderiliyor
    }

    // Sunucudan "sequenceNumber X'e kadar olan durumun sonucu şu" paketi geldiğinde
    void onServerReconciliation(uint32_t acknowledgedSeq, const Vector3& serverPosition) {
        // Sunucunun onayladığı komutları listeden temizle
        pendingCommands.erase(
            std::remove_if(pendingCommands.begin(), pendingCommands.end(),
                [&](auto& c) { return c.sequenceNumber <= acknowledgedSeq; }),
            pendingCommands.end()
        );

        // Karakteri sunucunun "gerçek" dediği pozisyona ışınla...
        localCharacter->position = serverPosition;

        // ...ve henüz sunucudan onay gelmemiş (hâlâ "pending" olan) komutları TEKRAR uygula
        for (auto& cmd : pendingCommands) {
            applyMovementLocally(localCharacter, cmd); // ★ Bu "replay" adımı — oyuncu bir sıçrama görmez
        }
    }

private:
    std::vector<InputCommand> pendingCommands;
    std::shared_ptr<Instance> localCharacter;
};
```

**Bu deseni anlamanın en kolay yolu:** İstemci sürekli "ben böyle olacağını tahmin ediyorum" diyerek yaşıyor. Sunucudan gerçek cevap geldiğinde, eğer tahmin doğruysa hiçbir şey değişmiyor (görsel olarak fark edilmez). Eğer tahmin yanlışsa (örn. bir duvara çarpmışsın ama istemci bunu bilmiyordu), karakter sunucunun dediği yere ışınlanıp, henüz cevaplanmamış son hareketleri tekrar oynatarak (replay) mevcut ana kadar "yakalanıyor" — bu sırada oyuncu rahatsız edici bir sıçrama görmüyor çünkü replay çok hızlı (tek karede) oluyor.

**Önemli mimari not:** Prediction sadece **yerel oyuncunun kendi karakteri** için yapılır. Diğer oyuncuların karakterleri için (Faz 6 MVP'de) basit interpolasyon yeterli — onların karakterleri zaten "geçmişteki" sunucu verisini gösteriyor, prediction gerektirmiyor. Bu ayrım karmaşıklığı önemli ölçüde azaltıyor.

---

## Bölüm E — RemoteEvent: Script'ten Sunucu-İstemci Mesajlaşması

### E.1 Neden ayrı bir mekanizma gerekiyor?

Replication (Bölüm C), **property değişikliklerini** otomatik senkronize ediyor. Ama bazı durumlarda script'in doğrudan "sunucuya bir mesaj gönder" demesi gerekiyor (örn. "Bu oyuncu mağazadan kılıç satın almak istiyor"). Bu, property değişikliği değil, tek seferlik bir olay — Roblox'taki `RemoteEvent` deseni tam olarak bunun için var.

### E.2 Luau tarafında görünüm

```lua
-- Sunucu scripti
local remote = Instance.new("RemoteEvent")
remote.Name = "BuySword"
remote.Parent = workspace

remote.OnServerEvent:Connect(function(player, itemName)
    -- Sunucu burada "gerçek" mantığı çalıştırır (para kontrolü, envanter güncelleme)
end)

-- İstemci scripti
workspace.BuySword:FireServer("IronSword")
```

### E.3 C++ tarafında binding — Faz 3'ün genişletilmesi

```cpp
class RemoteEvent : public Instance {
public:
    Signal onServerEvent; // Sunucu tarafında dinlenir
    Signal onClientEvent; // İstemci tarafında dinlenir

    void fireServer(std::vector<std::any> args) {
        if (NetworkContext::mode() != NetworkMode::Client) return; // Sadece istemci çağırabilir

        RemoteEventPacket packet;
        packet.remoteInstanceId = getInstanceId();
        packet.args = serializeArgs(args);
        NetworkClient::instance().send(NetChannel::Reliable_Ordered, packet);
    }

    void fireClient(const std::shared_ptr<Player>& target, std::vector<std::any> args) {
        if (NetworkContext::mode() != NetworkMode::Server) return; // Sadece sunucu çağırabilir

        RemoteEventPacket packet;
        packet.remoteInstanceId = getInstanceId();
        packet.args = serializeArgs(args);
        NetworkServer::instance().sendTo(target->getConnection(), NetChannel::Reliable_Ordered, packet);
    }
};

// Reflection kaydı — Faz 1.5'teki method() API'si (Faz 1.5, Bölüm D) burada kullanılıyor
ClassBuilder<RemoteEvent>("RemoteEvent")
    .base("Instance")
    .method("FireServer", &RemoteEvent::fireServer)
    .method("FireClient", &RemoteEvent::fireClient);
```

Sunucu bir `RemoteEventPacket` aldığında:

```cpp
void NetworkServer::handleRemoteEventPacket(HSteamNetConnection conn, const RemoteEventPacket& packet) {
    auto remote = InstanceRegistry::instance().findById(packet.remoteInstanceId);
    auto player = getPlayerForConnection(conn); // ★ Kritik: hangi oyuncunun gönderdiği biliniyor

    std::vector<std::any> args = deserializeArgs(packet.args);
    args.insert(args.begin(), player); // Luau tarafında ilk parametre her zaman "player" olur

    static_cast<RemoteEvent*>(remote.get())->onServerEvent.fire(args); // Faz 3'teki Signal, Faz 5'teki Touched ile birebir aynı mekanizma
}
```

**Güvenlik notu:** Sunucu, `FireServer` ile gelen her mesajı **asla körü körüne güvenmemeli**. Örneğin `BuySword` örneğinde sunucu, oyuncunun gerçekten yeterli parası olup olmadığını kendisi kontrol etmeli — istemci "satın aldım" dese bile sunucu bunu doğrulamadan kabul etmemeli. Bu, sunucu-otoriteli mimarinin (Bölüm A) temel gerekçesidir.

---

## Bölüm F — Faz 6 "Definition of Done" Kontrol Listesi

- [ ] `NetworkMode::Server` ve `NetworkMode::Client` modları ayrı çalıştırılabiliyor (aynı executable, farklı komut satırı parametresiyle)
- [ ] GNS ile istemci-sunucu bağlantısı kuruluyor, kopma/yeniden bağlanma senaryoları test edilmiş
- [ ] Sunucuda oluşturulan bir Part, otomatik olarak istemcide de beliriyor (obje replication)
- [ ] Sunucuda `part.Position` değiştirildiğinde istemcide de (20Hz gecikmeyle) güncelleniyor
- [ ] `noReplicate()` işaretli bir property'nin ağ paketlerinde **hiç** yer almadığı doğrulanmış (paket içeriği loglanarak kontrol edilmeli)
- [ ] Relevancy filtresi çalışıyor — uzak bir Part'ın güncellemeleri, o bölgeden uzak bir istemciye gönderilmiyor
- [ ] Client-side prediction çalışıyor — 100ms+ yapay gecikme (network simulation) altında bile yerel karakter hareketi anlık hissediliyor
- [ ] Reconciliation doğru çalışıyor — sunucu tahmin edilenden farklı bir sonuç bildirdiğinde karakter fark edilir bir sıçrama yapmadan düzeliyor
- [ ] `RemoteEvent:FireServer()` ve `:FireClient()` çalışıyor, sunucu tarafında `player` parametresinin doğru geldiği doğrulanmış
- [ ] Sunucu, istemciden gelen bir `RemoteEvent` verisini doğrulamadan (validation) doğrudan uygulamıyor — en az bir örnek senaryoda (örn. sahte/hileli veri gönderimi) bu test edilmiş

---

## Sonraki Adım Önerisi

Faz 6 ile proje artık gerçek anlamda "Roblox tarzı" bir multiplayer deneyim sunabiliyor. Üç yön öneriyorum:

1. **Karakter kontrolcüsü (Character Controller):** Fizik (Faz 5) ve networking (Faz 6) artık hazır olduğuna göre, Jolt'un `CharacterVirtual` sınıfı üzerine bir `Humanoid` sistemi inşa etmek — yürüme, zıplama, prediction ile entegre çalışması gereken en karmaşık gameplay sistemlerinden biri.
2. **Faz 3'teki script timeout konusu:** Artık hem fizik hem networking devrede, bir script'in sonsuz döngüye girmesi tüm sunucuyu (ve ona bağlı tüm oyuncuları) etkileyebilir — önceliği en yüksek seviyeye çıktı.
3. **Interest Management'in derinleştirilmesi:** Bölüm C.4'te bilinçli olarak basit tutulan mesafe-tabanlı relevancy sistemini, gerçek bir "Replication Graph" seviyesine taşımak (yüzlerce eş zamanlı oyuncuyu destekleyebilmek için).

Notunu aldım — tüm fazları bitirdiğimizde daha önce bıraktığımız diğer seçenekleri (Faz 2'de asset/post-processing, Faz 4'te Asset Browser, vb.) tek tek toparlarız.
