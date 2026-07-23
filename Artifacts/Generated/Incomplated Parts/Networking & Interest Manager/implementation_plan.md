# Faz 6 Networking & Faz 16 Interest Management

Bu plan, `Faz6_Networking.md` ve `Interest_Management_Derinlestirme.md` dokümanlarındaki gereksinimleri karşılamak amacıyla sunucu-otoriteli çok oyunculu mimariyi, replikasyon grafiğini (spatial grid) ve istemci tarafı tahmini (client-side prediction) özelliklerini motora eklemeyi hedefler.

## User Review Required

> [!IMPORTANT]
> **Valve GameNetworkingSockets (GNS):** Dokümanda GNS kullanılması gerektiği belirtilmiş. Ancak GNS, kaynak kodundan CMake ile derlenirken OpenSSL ve Protobuf gibi ağır bağımlılıklar gerektirir (Windows üzerinde statik derlemesi oldukça sancılı olabilir). 
> **Soru:** GNS yerine yine kanal tabanlı (reliable/unreliable) çalışan daha hafif bir C++ kütüphanesi (örn. **ENet** veya **Yojimbo**) kullanmayı onaylar mısınız? Yoksa GNS için ısrarcı mıyız (bu durumda vcpkg veya önceden derlenmiş binary'ler gerekebilir)?

> [!WARNING]
> **Reflection Entegrasyonu:** Property setter'larına `markPropertyDirty` mantığını bağlamak için `ClassBuilder` ve `PropertyDescriptor` yapılarına `InstanceId` ve `replicated` flag'i eklememiz gerekecek. Bu, mevcut DataModel kodlarında ufak çaplı değişiklikler anlamına geliyor.

## Open Questions

> [!TIP]
> **Replication Interval:** Saniyede 20 kare (20Hz) (yani `0.05f` deltaTime) olarak belirlenmiş. Bunun motor FPS'inden bağımsız, ayrı bir sabit tick rate (fixed update) içerisinde mi güncellenmesini istersiniz yoksa mevcut Render loop içerisinde zamanlayıcı (timer) ile mi?

## Proposed Changes

---
### ThirdParty Dependencies

#### [MODIFY] [ThirdParty/CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/ThirdParty/CMakeLists.txt)
- GNS (veya onaylanacaksa ENet/Yojimbo) kütüphanesi FetchContent ile çekilecek.
- Gerekli CMake yapılandırmaları yapılacak.

---
### Engine/Core Düzenlemeleri

#### [MODIFY] [Engine/Core/Reflection/TypeRegistry.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Reflection/TypeRegistry.h) & `ClassBuilder.h`
- `PropertyDescriptor` içerisine `bool replicated = true;` eklenecek.
- `ClassBuilder::noReplicate()` metodu eklenecek.
- Setter metodları tetiklendiğinde `ReplicationManager::instance().markPropertyDirty` çağrısı yapılacak.

#### [MODIFY] [Engine/Core/DataModel/Instance.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Instance.h)
- `bool alwaysRelevant = false;` bayrağı eklenecek (Interest Management için).

#### [NEW] [Engine/Core/DataModel/RemoteEvent.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/RemoteEvent.h) & `.cpp`
- Scriptlerden sunucu/istemci mesajlaşması için `FireServer` ve `FireClient` metodlarına sahip yeni bir `Instance` türevi eklenecek.

---
### Engine/Networking Modülü (Yeni)

#### [MODIFY] [Engine/CMakeLists.txt](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/CMakeLists.txt)
- `add_subdirectory(Networking)` eklenecek.

#### [NEW] [Engine/Networking/Transport/NetworkContext.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkContext.h)
- `enum class NetworkMode { Standalone, Server, Client };` tanımlanacak.

#### [NEW] [Engine/Networking/Transport/NetworkServer.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkServer.h) & `NetworkClient.h`
- Bağlantı açma, dinleme, paket alma, reliable/unreliable kanallar üzerinden paket gönderme işlemleri.

#### [NEW] [Engine/Networking/Replication/ReplicationManager.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Replication/ReplicationManager.h) & `.cpp`
- Değişen property'leri toplayıp (`dirtyProperties`) belirli aralıklarla istemcilere (veya istemciden sunucuya) `ReplicationPacket` gönderen sistem.

#### [NEW] [Engine/Networking/Replication/SpatialGrid.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Replication/SpatialGrid.h) & `RelevancyTracker.h`
- İlgi Yönetimi (Interest Management): Sahneyi 100x100 hücrelere bölme, Hysteresis (ENTER_RADIUS / EXIT_RADIUS) ile objelerin görünürlük hesaplamaları.

#### [NEW] [Engine/Networking/Replication/DormancyManager.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Replication/DormancyManager.h) & `PriorityCalculator.h`
- Uzun süre hareketsiz kalan objeleri uykuya alma ve mesafe/hız bazlı bant genişliği optimizasyonu (paket bütçeleme).

#### [NEW] [Engine/Networking/Prediction/ClientPredictor.h](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Prediction/ClientPredictor.h)
- Oyuncunun yerel input'unu anında uygulayıp, sunucudan gelen `sequenceNumber` ile reconciliation (düzeltme/replay) yapan sistem.

---
### Editor ve Başlatma

#### [MODIFY] [Editor/Main.cpp](file:///C:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/Main.cpp)
- Komut satırı argümanları parse edilecek (`--server`, `--client`). Belirtilmediyse `Standalone` modu seçilecek.
- Geliştirici UI'sına Networking sekmesi eklenip "Host Server" veya "Connect to localhost" düğmeleri konulacak.

## Verification Plan
1. **Bağlantı Testi**: Aynı bilgisayarda iki Editor/Uygulama açıp birini Server, diğerini Client yaparak birbirine bağlamak.
2. **Replication Testi**: Server'daki Explorer'dan bir Part'ın pozisyonunu veya rengini değiştirince anında Client'a yansıdığını gözlemlemek.
3. **Interest Management**: Uzağa gidildiğinde Part'ların Relevancy listesinden düştüğünü loglamak.
4. **Client Prediction**: Yapay ping (simüle edilmiş gecikme) altında istemci objesinin gecikmesiz hareket edebildiğini ve sunucu doğrulamasını görebilmek.
5. **RemoteEvent Testi**: Luau üzerinden `FireServer` çağrısı yaparak sunucuda tetiklenmesini sınamak.
