# Faz 5 — Teknik Derinlemesine İnceleme
## Fizik: Jolt Physics Entegrasyonu

Bu doküman, Faz 1-4'te kurulan DataModel/Render/Scripting/Editör sistemlerinin üzerine Jolt Physics'in nasıl entegre edileceğini inceler. Hedef: `part.Anchored = false` yazıldığında objenin yerçekimiyle düşmesi, iki Part çarpıştığında `Touched` event'inin (Faz 3'te tasarlanmıştı) gerçekten tetiklenmesi.

---

## Bölüm A — Kritik Mimari Karar: Üçüncü Bir Paralel Sistem Daha

### A.1 Artık tanıdık bir desen

Faz 2'de DataModel (OOP ağaç) ile RenderScene (düz proxy dizisi) arasında bir ayrım kurmuştuk. Fizik için de **aynı desen** tekrarlanıyor — çünkü Jolt Physics de kendi iç dünyasında (bir "PhysicsSystem" içinde) rigid body'leri düz, cache-friendly dizilerde tutuyor, bizim `Instance` ağacımızdan tamamen habersiz.

```
┌─────────────────┐     property değişince      ┌──────────────────┐     her fizik adımında    ┌───────────────┐
│  DataModel (OOP)  │ ───── senkronize eder ────▶│  Jolt BodyID      │◀──── senkronize eder ────│  Jolt Physics  │
│                   │                              │  (Part başına 1)  │                            │  System        │
│  Part.position    │◀──── fizik sonucu geri ─────│                    │                            │  (kendi thread'i)│
│  Part.anchored    │       yazılır                │                    │                            │                │
└─────────────────┘                              └──────────────────┘                            └───────────────┘
```

**Önemli fark:** Render senkronizasyonu tek yönlüydü (DataModel → RenderScene). Fizik senkronizasyonu **çift yönlü**: Script `part.Position` değiştirdiğinde bu Jolt'a yazılmalı, ama Jolt her fizik adımında (çarpışma, yerçekimi sonucu) objeyi hareket ettirdiğinde bu da geri DataModel'e yazılmalı. Bu çift yönlü senkronizasyon, Faz 5'in en hassas noktası.

### A.2 Neden `Instance`'a doğrudan Jolt body pointer'ı koymuyoruz?

İlk akla gelen naif yaklaşım, `Part` sınıfına `JPH::Body* physicsBody` gibi bir alan eklemek olurdu. Bunun sorunları:

- Jolt'un kendi bellek yönetimi var (`BodyID` bir handle, ham pointer değil) — Jolt body'leri kendi içinde yeniden düzenleyebilir (defragmentation).
- `Part` sınıfını doğrudan Jolt'a bağımlı hale getirmek, Faz 1'deki "Engine/Core, hiçbir üçüncü parti kütüphaneye bağımlı olmamalı" prensibini bozar — ileride fizik motorunu değiştirmek (örn. Jolt'tan başka bir şeye geçmek) imkânsız hale gelir.

Bunun yerine Faz 2'deki `RenderProxy` deseninin birebir aynısını kuruyoruz: `Part`, bir `PhysicsBodyHandle` (basit bir `uint32_t`) tutar, gerçek Jolt nesnesine hiç dokunmaz.

```cpp
// Engine/Physics/PhysicsBodyHandle.h
using PhysicsBodyHandle = uint32_t;
constexpr PhysicsBodyHandle InvalidPhysicsHandle = 0xFFFFFFFF;
```

---

## Bölüm B — PhysicsWorld: Jolt'un Sarmalanması (Wrapping)

### B.1 Jolt'un temel kavramları (kısa özet)

Jolt Physics üç ana kavram üzerine kurulu:
- **BodyInterface:** Rigid body oluşturma/silme/güncelleme için ana API.
- **Layers:** Hangi objelerin hangileriyle çarpışacağını tanımlayan katman sistemi (örn. "Static" katmanı kendisiyle çarpışmaz, sadece "Dynamic" ile çarpışır).
- **ContactListener:** Çarpışma olaylarını dinleyen callback arayüzü — bizim `Touched` event'imiz buradan besleniyor.

### B.2 PhysicsWorld sarmalayıcısı

```cpp
// Engine/Physics/PhysicsWorld.h

#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

class PhysicsWorld {
public:
    void initialize() {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        physicsSystem.Init(
            maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
            broadPhaseLayerInterface, objectVsBroadPhaseFilter, objectLayerPairFilter
        );

        physicsSystem.SetContactListener(&contactListener); // ★ Bölüm D
        physicsSystem.SetGravity(JPH::Vec3(0, -9.81f * studsPerMeter, 0));
    }

    // Her karede (veya sabit bir fizik tick-rate'inde) çağrılır
    void step(float deltaTime) {
        constexpr float fixedTimeStep = 1.0f / 60.0f;
        accumulator += deltaTime;

        while (accumulator >= fixedTimeStep) {
            physicsSystem.Update(fixedTimeStep, /*collisionSteps=*/1, &tempAllocator, &jobSystem);
            accumulator -= fixedTimeStep;
        }
    }

    JPH::BodyInterface& getBodyInterface() { return physicsSystem.GetBodyInterface(); }

private:
    JPH::PhysicsSystem physicsSystem;
    float accumulator = 0.0f;
    static constexpr float studsPerMeter = 1.0f; // Roblox tarzı "stud" birimi için ölçek (Bölüm F)
    ContactListenerImpl contactListener;
    // ... allocator, jobSystem, layer filter üyeleri
};
```

**Neden sabit zaman adımı (`fixedTimeStep`) kullanılıyor?** Değişken frame rate'te fizik simülasyonu çalıştırmak, farklı bilgisayarlarda farklı sonuçlar üretir (60 FPS'te bir obje 2 metre düşerken, 30 FPS'te farklı davranabilir). Sabit adım + accumulator deseni (Gaffer On Games'in "Fix Your Timestep" makalesinde popülerleşen yöntem), fizik simülasyonunu render frame rate'inden bağımsızlaştırıyor — bu, Unreal ve Unity'nin de kullandığı standart yaklaşım.

---

## Bölüm C — Part ⟷ Jolt Body Senkronizasyonu

### C.1 Body oluşturma — Part sahneye eklendiğinde

```cpp
// Part.h içine eklenen genişletme (Faz 2'deki onAddedToWorkspace'in devamı)

void Part::onAddedToWorkspace() {
    // ... Faz 2'deki RenderProxy oluşturma kodu ...

    JPH::BodyCreationSettings bodySettings(
        new JPH::BoxShape(toJoltVec3(size * 0.5f)),
        toJoltVec3(position),
        toJoltQuat(rotation),
        anchored ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        anchored ? Layers::STATIC : Layers::DYNAMIC
    );
    bodySettings.mUserData = (uint64_t)getInstanceId(); // ★ Kritik: ContactListener bu Part'ı geri bulacak

    JPH::BodyID bodyId = PhysicsWorld::instance().getBodyInterface().CreateAndAddBody(
        bodySettings, JPH::EActivation::Activate
    );
    physicsBodyHandle = PhysicsBodyRegistry::instance().registerBody(getInstanceId(), bodyId);
}
```

`mUserData` alanına `InstanceId` yazmak kritik bir detay — Jolt bir çarpışma bildirdiğinde bize sadece `BodyID` veriyor, biz bunun **hangi `Part`'a** karşılık geldiğini bilmek zorundayız. Bu geri-eşleme olmadan `Touched` event'i hangi Instance üzerinde `fire()` edileceğini bilemez.

### C.2 Senkronizasyon Yönü 1: Script → Jolt (property setter üzerinden)

```cpp
void Part::setPosition(const Vector3& newPos) {
    position = newPos;
    markRenderDirty(); // Faz 2

    // ★ Yeni: Jolt'a da haber ver
    if (physicsBodyHandle != InvalidPhysicsHandle) {
        JPH::BodyID bodyId = PhysicsBodyRegistry::instance().getBodyId(physicsBodyHandle);
        PhysicsWorld::instance().getBodyInterface().SetPosition(
            bodyId, toJoltVec3(newPos), JPH::EActivation::Activate
        );
    }
}
```

Bu, Faz 1'deki `PropertyDescriptor::setter` mekanizmasının üçüncü kez faydasını gösterdiği yer: `part.Position = ...` çağrısı hâlâ tek bir reflection setter'ından geçiyor, ama artık bu setter üç şeyi aynı anda tetikliyor — DataModel güncelleme, RenderProxy dirty işaretleme, ve Jolt body pozisyon güncelleme.

### C.3 Senkronizasyon Yönü 2: Jolt → DataModel (her fizik adımından sonra)

Bu yön daha hassas çünkü **her Part için tek tek senkronize etmek** (Faz 2'nin `RenderScene` deseninde olduğu gibi düz bir dizi gezmek) gerekiyor:

```cpp
// PhysicsWorld::step() içinde, physicsSystem.Update()'ten hemen sonra

void PhysicsWorld::syncBodiesToDataModel() {
    JPH::BodyIDVector activeBodies;
    physicsSystem.GetActiveBodies(JPH::EBodyType::RigidBody, activeBodies); // ★ Sadece HAREKET EDEN body'ler

    for (JPH::BodyID bodyId : activeBodies) {
        InstanceId ownerId = (InstanceId)bodyInterface.GetUserData(bodyId);
        auto part = InstanceRegistry::instance().findById(ownerId);
        if (!part) continue;

        JPH::RVec3 joltPos = bodyInterface.GetPosition(bodyId);
        part->position = fromJoltVec3(joltPos); // ★ Doğrudan alan ataması — setPosition() ÇAĞRILMIYOR
        part->markRenderDirty();                 // Ama render güncellemesi manuel tetikleniyor
    }
}
```

**Neden burada `setPosition()` çağrılmıyor, doğrudan `position` alanına yazılıyor?** Çünkü `setPosition()` Jolt'a da yazma yapıyordu (Bölüm C.2) — eğer Jolt'un bize söylediği sonucu tekrar Jolt'a yazsaydık, gereksiz bir geri-besleme döngüsü (feedback loop) oluşurdu. Bu, çift yönlü senkronizasyon sistemlerinde çok sık karşılaşılan bir tuzaktır: **"kaynağından gelen veri, tekrar kaynağa yazılmamalı."**

`GetActiveBodies()` kullanımı da performans açısından önemli — uykuda olan (sleeping), hareket etmeyen binlerce Static/durgun Part için bu döngü hiç çalışmıyor, sadece o karede gerçekten hareket eden body'ler işleniyor.

---

## Bölüm D — Touched Event: ContactListener'dan Signal'e

### D.1 Jolt ContactListener implementasyonu

```cpp
// Engine/Physics/ContactListenerImpl.h

class ContactListenerImpl : public JPH::ContactListener {
public:
    void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2,
                          const JPH::ContactManifold& manifold,
                          JPH::ContactSettings& settings) override {
        InstanceId id1 = (InstanceId)body1.GetUserData();
        InstanceId id2 = (InstanceId)body2.GetUserData();

        // ★ Fizik thread'inden gelen bu callback, doğrudan Luau/DataModel'e dokunmuyor —
        // bunun yerine bir kuyruğa yazıyor (Bölüm D.2'de neden açıklanıyor)
        PendingContactEvents::instance().enqueue({id1, id2});
    }
};
```

### D.2 Neden doğrudan `fire()` çağrılmıyor — Thread Safety

Jolt Physics, `physicsSystem.Update()` çağrısı sırasında **kendi iş parçacıklarında** (job system) çalışır ve `ContactListener` callback'leri bu worker thread'lerinden tetiklenebilir. Ama bizim `Signal::fire()` mekanizmamız (Faz 3) ve Luau VM'i **tek thread'li** çalışacak şekilde tasarlandı (script'ler ana thread'de coroutine olarak yürüyor). Fizik thread'inden doğrudan Luau'ya dokunmak veri yarışına (race condition) yol açar.

Çözüm: Bir **thread-safe kuyruk (queue)** kullanmak — fizik thread'i sadece kuyruğa event ekliyor, ana thread her karede bu kuyruğu boşaltıp gerçek `fire()` çağrılarını güvenli şekilde yapıyor:

```cpp
// Ana motor döngüsünde, PhysicsWorld::step()'ten SONRA, script update'ten ÖNCE

void GameLoop::processPhysicsEvents() {
    std::vector<ContactEvent> events = PendingContactEvents::instance().drainAll(); // thread-safe pop

    for (auto& evt : events) {
        auto part1 = InstanceRegistry::instance().findById(evt.id1);
        auto part2 = InstanceRegistry::instance().findById(evt.id2);
        if (!part1 || !part2) continue; // Bu karede silinmiş olabilirler — güvenli kontrol şart

        part1->touchedSignal.fire({part2}); // ★ Artık ana thread'deyiz, güvenli
        part2->touchedSignal.fire({part1});
    }
}
```

Bu, Faz 3'te tasarlanan `Signal` sisteminin tam olarak neden var olduğunun kanıtı: Fizik motoru sadece `fire()` çağırıyor, `Signal` sınıfı kimin dinlediğinden (C++ kod mu, Luau script mi) habersiz.

---

## Bölüm E — Constraint API'leri (WeldConstraint örneği)

Roblox'un `WeldConstraint`, `HingeConstraint` gibi yapıları, Jolt'un kendi constraint sistemine (`JPH::Constraint` türevleri) ince bir sarmalayıcı olarak inşa ediliyor. Faz 1.5'te tasarlanan **Object Reference** property tipi (weak_ptr tabanlı) burada devreye giriyor:

```cpp
// Engine/Core/DataModel/WeldConstraint.h

class WeldConstraint : public Instance {
public:
    std::weak_ptr<Instance> part0; // Faz 1.5, Bölüm B.4
    std::weak_ptr<Instance> part1;

    void onEnabled() { // İkisi de atandığında constraint gerçek Jolt nesnesine dönüşür
        auto p0 = std::dynamic_pointer_cast<Part>(part0.lock());
        auto p1 = std::dynamic_pointer_cast<Part>(part1.lock());
        if (!p0 || !p1) return;

        JPH::FixedConstraintSettings settings;
        settings.mAutoDetectPoint = true; // Jolt, iki body'nin şu anki konumuna göre otomatik hesaplar

        JPH::BodyID id0 = PhysicsBodyRegistry::instance().getBodyId(p0->physicsBodyHandle);
        JPH::BodyID id1 = PhysicsBodyRegistry::instance().getBodyId(p1->physicsBodyHandle);

        joltConstraint = settings.Create(
            *bodyInterface.GetBody(id0), *bodyInterface.GetBody(id1)
        );
        PhysicsWorld::instance().addConstraint(joltConstraint);
    }

private:
    JPH::Constraint* joltConstraint = nullptr;
};

// Reflection kaydı — Faz 1.5'teki objectProperty() burada kullanılıyor
ClassBuilder<WeldConstraint>("WeldConstraint")
    .base("Instance")
    .objectProperty("Part0", &WeldConstraint::part0)
    .objectProperty("Part1", &WeldConstraint::part1);
```

---

## Bölüm F — Ölçek Birimi Kararı: "Stud" Sistemi

### F.1 Neden bu bir tasarım kararı gerektiriyor?

Jolt Physics (çoğu fizik motoru gibi) **metre** birimini varsayar ve varsayılan ayarları (yerçekimi ivmesi, sürtünme katsayıları, solver hassasiyeti) buna göre kalibre edilmiştir. Roblox'un "stud" birimi ise 1 stud ≈ 0.28 metre gibi farklı bir ölçekte. Eğer Part boyutlarını doğrudan "stud" değeri olarak Jolt'a verirsek (örn. bir Part'ın boyu "4 stud" iken bunu "4 metre" sanarak işlerse), simülasyon kararsız hale gelebilir (çok küçük veya çok büyük objelerde Jolt'un sayısal hassasiyeti bozulur).

**Karar:** Motor içinde tüm pozisyon/boyut değerleri "stud" biriminde tutulacak (Roblox deneyimine sadık kalmak için), ama Jolt'a veri gönderilirken/alınırken bir dönüşüm katsayısı (`studsPerMeter`) uygulanacak. Bu dönüşüm, `toJoltVec3()`/`fromJoltVec3()` yardımcı fonksiyonlarında merkezi olarak yapılıyor (Bölüm C'de görülen fonksiyonlar) — böylece geri kalan tüm motor kodu "stud" dünyasında kalabiliyor, sadece fizik köprüsü ölçek dönüşümünü biliyor.

```cpp
// Engine/Physics/PhysicsConversions.h
constexpr float STUDS_PER_METER = 3.57f; // ~ Roblox'un gerçek oranına yakın bir değer

inline JPH::Vec3 toJoltVec3(const Vector3& studs) {
    return JPH::Vec3(studs.x / STUDS_PER_METER, studs.y / STUDS_PER_METER, studs.z / STUDS_PER_METER);
}
inline Vector3 fromJoltVec3(const JPH::RVec3& meters) {
    return Vector3(meters.GetX() * STUDS_PER_METER, meters.GetY() * STUDS_PER_METER, meters.GetZ() * STUDS_PER_METER);
}
```

---

## Bölüm G — Faz 5 "Definition of Done" Kontrol Listesi

- [ ] `PhysicsWorld` başlatılıyor, sabit zaman adımıyla (`fixedTimeStep`) her karede güncelleniyor
- [ ] `Part.Anchored = false` olan bir obje yerçekimiyle düşüyor, `Anchored = true` olan sabit kalıyor
- [ ] Script'ten `part.Position` değiştirildiğinde Jolt body'si de doğru güncelleniyor (Yön 1)
- [ ] Fizik simülasyonu bir objeyi hareket ettirdiğinde bu DataModel'e ve dolayısıyla RenderProxy'ye yansıyor (Yön 2), **feedback loop oluşmadan**
- [ ] Sadece aktif (hareket eden) body'ler her karede senkronize ediliyor — 1000 durgun Part'lı bir sahnede performans testiyle doğrulanmış
- [ ] `part.Touched:Connect()` ile bağlanmış bir Luau fonksiyonu, gerçek bir çarpışmada tetikleniyor
- [ ] Contact event'lerin fizik thread'inden ana thread'e **thread-safe bir kuyrukla** aktarıldığı doğrulanmış (race condition testi / ThreadSanitizer ile)
- [ ] `WeldConstraint` iki Part'ı gerçekten birbirine sabitliyor, biri hareket ettirildiğinde diğeri de takip ediyor
- [ ] Stud ⟷ metre dönüşümü doğru çalışıyor — çok küçük (0.1 stud) veya çok büyük (10.000 stud) objelerde simülasyon kararsızlaşmıyor

---

## Sonraki Adım Önerisi

Fizik entegrasyonuyla birlikte proje artık gerçek bir "oyun" hissi vermeye başlıyor — objeler düşüyor, çarpışıyor, script'ler tepki veriyor. Sıradaki mantıklı adımlar:

1. **Faz 6 — Networking:** Şu ana kadarki her şey tek oyunculu. Roblox'un temel değer önerisi multiplayer olduğu için, DataModel replication'ın nasıl çalışacağı kritik bir sonraki adım.
2. **Karakter kontrolcüsü (Character Controller):** Fizik motoru kuruldu ama henüz bir "oyuncu" kavramı yok — yürüme, zıplama, `Humanoid` benzeri bir sistem ayrı bir tasarım gerektirir (Jolt'un `CharacterVirtual` sınıfı bunun için var).
3. **Faz 3'teki script timeout konusu:** Hâlâ ele alınmadı, artık fizik de eklendiği için bir sonsuz döngü artık hem script'i hem fizik adımını kilitleyebilir — önceliği artmış olabilir.

Hangisiyle devam edelim?
