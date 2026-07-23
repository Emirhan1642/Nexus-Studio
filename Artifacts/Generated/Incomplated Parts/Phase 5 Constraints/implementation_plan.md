# Constraints Entegrasyonu (Faz 5)

Bu plan, Roblox'taki `WeldConstraint` mantığıyla Jolt Physics üzerinde mekanik kısıtlamaların (Constraints) altyapısını kurmayı ve ilk kısıtlamamızı eklemeyi içerir. 

## User Review Required

> [!IMPORTANT]
> `std::weak_ptr` tabanlı *Object Reference* propertiy'leri (Part0, Part1) Reflection sistemine eklenecektir. Bu sayede bellek sızıntısı olmadan iki Part objesini birbirine bağlayabileceğiz.

## Open Questions

> [!NOTE]
> 1. Bu aşamada sadece `WeldConstraint` mi ekleyelim, yoksa hemen ardından `HingeConstraint` (Menteşe) ve `SpringConstraint` (Yay) gibi diğerlerini de dahil edelim mi? İlk aşamada sadece `Weld` ile sistemi test etmek daha güvenli olabilir.
> 2. Constraint'leri sahne ekranında görselleştirmek (iki Part arasına çizgi çekmek vs.) ister misiniz, yoksa şimdilik sadece fiziksel olarak çalışması ve Properties panelinde gözükmesi yeterli mi?

## Proposed Changes

---

### Core & Reflection Updates

#### [MODIFY] `Engine/Core/Reflection/ClassBuilder.h`
- `objectPropertyAccessor` adında yeni bir metod eklenecek. Bu sayede `Part0` veya `Part1` değiştiğinde `setPart0` üzerinden C++ tarafında bir olay tetikleyebileceğiz (Eski kısıtlamayı silip yenisini yaratmak için).

#### [MODIFY] `Engine/Core/DataModel/Part.h`
- Jolt Constraint'leri oluştururken nesnenin Body ID'sine ihtiyacımız var. `uint32_t physicsBodyId` alanını okumak için public bir `getPhysicsBodyId()` metodu eklenecek.

---

### Physics System Updates

#### [MODIFY] `Engine/Physics/PhysicsWorld.h`
- Jolt'a constraint ekleyip çıkarmak için public metodlar eklenecek:
  - `void addConstraint(JPH::Constraint* constraint);`
  - `void removeConstraint(JPH::Constraint* constraint);`
- Constraint yaratırken `PhysicsSystem`'e erişmek gerektiği için `JPH::PhysicsSystem& getPhysicsSystem()` metodu eklenecek.

#### [MODIFY] `Engine/Physics/PhysicsWorld.cpp`
- Eklenen `addConstraint` ve `removeConstraint` fonksiyonlarının içi doldurulacak (direkt `physicsSystem.AddConstraint(...)`).

---

### Constraint Classes

#### [NEW] `Engine/Core/DataModel/WeldConstraint.h`
#### [NEW] `Engine/Core/DataModel/WeldConstraint.cpp`
- `Instance` sınıfından türeyen `WeldConstraint` yaratılacak.
- Özellikleri:
  - `std::weak_ptr<Instance> part0`
  - `std::weak_ptr<Instance> part1`
- `setPart0` ve `setPart1` metodları yazılacak. İki part da atandığında ve obje Workspace'te ise (aktifse) `JPH::FixedConstraintSettings` kullanılarak Jolt Constraint'i oluşturulacak.
- `onAddedToWorkspace` ve `onRemovedFromWorkspace` metodlarında Constraint'in Jolt dünyasına eklenip çıkarılması (cleanup) sağlanacak.
- `ClassBuilder<WeldConstraint>` ile özellikleri Editor Properties paneline eklenecek.

## Verification Plan

### Manual Verification
1. Editörde `Explorer` paneli üzerinden iki adet `Part` (Küp) ve bir adet `WeldConstraint` eklenecek.
2. `WeldConstraint`'in `Part0` özelliğine birinci küp, `Part1` özelliğine ikinci küp atanacak. (Properties panelindeki `ObjectRef` desteği test edilecek).
3. Küplerden sadece biri `Anchored = true` yapılacak.
4. Simülasyon başlatıldığında (F5), Anchored olmayan küpün yerçekimiyle düşmeyip, diğer küpe *sabitlenmiş (kaynaklanmış)* şekilde havada asılı kalıp kalmadığı doğrulanacak.
