# Nexus Studio — Eksiklikleri Düzeltme Planı

Önceki ajanın bıraktığı eksiklikleri ve sapmaları giderme planı. 4 ana başlık, öncelik sırasına göre.

## Önemli Notlar

> [!WARNING]
> Bu plan yalnızca **kaynak kodu değişikliklerini** kapsar — hiçbir build sistemi komutu çalıştırılmadan önce tüm dosyalar yazılacak, sonra tek seferde derleme yapılacak.

> [!IMPORTANT]
> **GNS Kararı:** ENet kaldırılıp GameNetworkingSockets (Valve/Open source) kullanılacak. GNS, dokümanda gerekçesiyle seçilmişti (UDP + seçilebilir güvenilirlik + şifreleme). ENet'in tüm referansları temizlenecek.

---

## Grup 1 — Reflection & Scripting Eksiklikleri (En Kritik)

### 1.1 Method Binding — Luau'dan C++ metodları çağırma

**Sorun:** `InstanceBinding.cpp` satır 97'de `// TODO: Methods, etc.` yazıyor. `ClassBuilder::method()` ile kayıtlı hiçbir metod Lua'dan çağrılamıyor (örn. `instance:Destroy()`, `humanoid:Jump()`).

#### [MODIFY] [InstanceBinding.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/InstanceBinding.cpp)
- `instance_index` içindeki `// TODO` satırını, `classDesc->methods` üzerinde dönen ve bulunan metodu `lua_pushcfunction` ile saran bir blokla değiştir.
- Metodun argümanlarını `luauValueToAny` ile topla, `MethodDescriptor::invoke` çağır, sonucu `pushAnyToLuau` ile döndür.

---

### 1.2 `task.*` API — Roblox uyumlu zamanlama

**Sorun:** Yalnızca eski `wait()` fonksiyonu var. `task.wait()`, `task.spawn()`, `task.delay()` eksik.

#### [MODIFY] [ScriptScheduler.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.h)
- `spawnThread(lua_State* thread)` metodu ekle (0 bekleme süresiyle kuyruğa alır).
- `delayThread(lua_State* thread, double duration)` metodu ekle.

#### [MODIFY] [ScriptScheduler.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.cpp)
- `luau_task_wait`, `luau_task_spawn`, `luau_task_delay` C fonksiyonları.

#### [MODIFY] [LuauVM.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/LuauVM.cpp)
- `registerEngineAPI()` içine `task` tablosu ekle: `task.wait`, `task.spawn`, `task.delay`.

---

## Grup 2 — InstanceRegistry (Kritik Altyapı)

**Sorun:** Faz 5'te `ContactListenerImpl` fizik çarpışmasından dönen `BodyID`'yi bir `Part`'a eşlemek için `InstanceRegistry::findById()` kullanıyor ama bu sınıf **hiç yazılmamış**. `getInstanceId()` şu an `reinterpret_cast<InstanceId>(this)` (ham pointer), bu da Networking için yetersiz.

#### [NEW] [Engine/Core/DataModel/InstanceRegistry.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/InstanceRegistry.h)
```cpp
// Tüm aktif Instance'ları ID → shared_ptr ile tutan global registry.
// ID olarak mevcut reinterpret_cast(this) kullanılmaya devam edilir (tutarlılık için).
class InstanceRegistry {
public:
    static InstanceRegistry& instance();
    void registerInstance(const std::shared_ptr<Instance>& inst);
    void unregisterInstance(InstanceId id);
    std::shared_ptr<Instance> findById(InstanceId id) const;
private:
    std::unordered_map<InstanceId, std::weak_ptr<Instance>> m_registry;
};
```

#### [MODIFY] [Engine/Core/DataModel/Instance.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Instance.cpp)
- `setParent()` içinde parent'a eklenince `InstanceRegistry::instance().registerInstance(shared_from_this())` çağır.
- `destroy()` içinde `InstanceRegistry::instance().unregisterInstance(getInstanceId())` çağır.

#### [MODIFY] [Engine/Core/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/CMakeLists.txt)
- `InstanceRegistry.cpp` dosyasını source listesine ekle.

---

## Grup 3 — GNS Entegrasyonu (Dokümandan Sapma Düzeltmesi)

**Sorun:** ENet kullanılmış, dokümanda GNS seçilmişti.

#### [MODIFY] [ThirdParty/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/ThirdParty/CMakeLists.txt)
- ENet bloğunu (`# 8. ENet`) sil.
- Yerine GameNetworkingSockets FetchContent bloğu ekle:
```cmake
FetchContent_Declare(
    GameNetworkingSockets
    GIT_REPOSITORY https://github.com/ValveSoftware/GameNetworkingSockets.git
    GIT_TAG        master
)
set(STEAMNETWORKINGSOCKETS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(GameNetworkingSockets)
```

#### [MODIFY] [Engine/Networking/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/CMakeLists.txt)
- Link target'ı `enet` → `GameNetworkingSockets` olarak değiştir.

#### [MODIFY] [Engine/Networking/Transport/NetworkServer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkServer.h)
- `#include <enet/enet.h>` → `#include <steam/isteamnetworkingsockets.h>`
- `ENetHost*`, `ENetPeer*` → `HSteamListenSocket`, `HSteamNetConnection` + Valve tipleri.
- API'yi GNS'e göre yeniden yaz (StartListening, Poll, SendTo, Broadcast).

#### [MODIFY] [Engine/Networking/Transport/NetworkServer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkServer.cpp)
- GNS callback tabanlı implementasyon.

#### [MODIFY] [Engine/Networking/Transport/NetworkClient.h & .cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Networking/Transport/NetworkClient.h)
- Aynı şekilde ENet → GNS geçişi.

---

## Grup 4 — Play/Stop Tam Serialization

**Sorun:** Şu an yalnızca `Part::position` kaydediliyor. Play modunda yeni eklenen objeler Stop'ta silinmiyor, silinen objeler geri gelmiyor.

#### [NEW] [Engine/Core/DataModel/DataModelSnapshot.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/DataModelSnapshot.h)
```cpp
// DataModel ağacının derin bir kopyasını (deep clone) tutan yapı.
// Play başında snapshot alınır, Stop'ta restore edilir.
struct PartSnapshot { std::string name; Vector3 position, size; bool anchored; /* PBR fields */ };
class DataModelSnapshot {
public:
    void capture(const std::shared_ptr<Instance>& root);
    void restore(std::shared_ptr<Instance>& root);
private:
    std::vector<PartSnapshot> m_parts;
};
```

#### [MODIFY] [Editor/Main.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/Main.cpp)
- `savedPositions` map'ini kaldır.
- `DataModelSnapshot snapshot;` ekle.
- Play'de `snapshot.capture(DataModel::instance())`, Stop'ta `snapshot.restore(...)`.

---

## Verification Plan

### Derleme Kontrolü
```powershell
cmake --preset windows-debug
cmake --build build --config Debug
```

### Otomatik Testler
```powershell
.\build\bin\Debug\NexusStudioTests.exe
```

Beklenti: Mevcut 6 reflection testi + yeni InstanceRegistry testi geçmeli.

### Manuel Doğrulama
1. **Method binding:** Script'ten `part:Destroy()` çağrısı objeyi silmeli.
2. **task.wait:** `task.wait(1)` çağrısı motoru dondurmadan 1 saniye beklemeli.
3. **InstanceRegistry:** Fizik çarpışması (F5 → Touched olayı) doğru Part'ı döndürmeli.
4. **Play/Stop tam restore:** Play modunda yeni Part eklendikten sonra Stop'a basılınca o Part silinmeli.
5. **GNS:** `--server` argümanıyla başlatıldığında 7777 portunu GNS ile dinlemeli.
