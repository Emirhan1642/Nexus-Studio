# Ragdoll Physics Integration (Phase 16)

Bu belge, `Humanoid` karakter kontrolcüsünde statik mock ragdoll implementasyonundan, dinamik iskelet tabanlı (Skeleton-based) gerçek ragdoll fizik entegrasyonuna geçişin planını sunmaktadır.

## User Review Required

> [!IMPORTANT]
> - Karakterin gerçek iskeleti (`Animation::Skeleton`) kullanılarak fiziksel gövdeler oluşturulacaktır. Her kemik (bone) için otomatik olarak Jolt `CapsuleShape` üretilmesi planlanıyor.
> - Jolt Physics'in doğrudan `JPH::Ragdoll` sınıfı yerine, daha fazla esneklik ve kod sadeliği sağlaması adına manuel olarak `JPH::Body` ve `JPH::SixDOFConstraint` (veya Hinge/ConeConstraint) kullanılarak kemik hiyerarşisi inşa edilecektir. Bu yaklaşım uygun mudur?

## Open Questions

> [!WARNING]
> Kemiklerin fiziksel çarpışma şekilleri (collision shapes) boyutları nasıl belirlenmeli?
> - **Seçenek A (Önerilen):** Her kemiğin boyutunu, parent'ından ilk child'ına olan mesafeye göre prosedürel olarak hesaplayalım (yaprak kemikler için default bir boyut).
> - **Seçenek B:** Asset içerisinden gelen verileri (PhysicsAsset gibi) kullanalım (Şu an böyle bir sistemimiz yok, efor gerektirir).

## Proposed Changes

### `Engine/Core/DataModel/Humanoid.h`
- `RagdollLimb` yapısını güncelleyeceğiz. Artık sahte `Part` referansları taşımak yerine, doğrudan kemik indeksini (`boneIndex`) ve Jolt `BodyID`sini tutacak.
- Animasyon ve fizik harmanlaması (blending) için ragdoll'dan çıkış yapıldığında pose'un nasıl kurtarılacağına dair altyapı hazırlanacak.

#### [MODIFY] [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/Humanoid.h)
- `RagdollLimb` struct'ı şu şekilde değişecek:
  ```cpp
  struct RagdollLimb {
      int boneIndex;
      JPH::BodyID bodyId;
  };
  ```

### `Engine/Core/DataModel/Humanoid.cpp`
- `enterRagdoll()` metodu tamamen yeniden yazılacak. Önceden sahte `Part` nesneleri oluşturan kod silinecek. Bunun yerine `skeleton.bones` üzerinde dönülerek:
  1. Her kemik için (Root/Pelvis'ten başlayarak) bir kapsül çarpışma şekli (`JPH::CapsuleShape`) oluşturulacak.
  2. Kemiğin dünya konumunda (world space) bir `JPH::Body` (Dynamic) oluşturulacak.
  3. Parent kemiği varsa, iki gövde birbirine `JPH::SwingTwistConstraint` veya `JPH::PointConstraint` (basitlik için başlangıçta Point) ile bağlanacak.
- `update()` metodunda, eğer state `Ragdoll` ise:
  - `animationPlayer.evaluate()` yerine, `RagdollLimb` listesi dönülerek Jolt gövdelerinden dünya dönüşümleri (`worldPose`) çekilecek ve GPU skinning matrisleri (`finalBoneTransforms`) fizik motoru tarafından belirlenecek.
- `exitRagdoll()` metodunda Jolt gövdeleri ve bağlantıları temizlenecek, animasyon sistemi kontrolü geri alacak.

## Verification Plan

### Automated Tests
- `HumanoidTests.cpp` içine `RagdollSimulation` adında yeni bir test eklenecek.
- Test, basit 3 kemikli bir `Skeleton` oluşturacak.
- `enterRagdoll()` çağrılacak ve fizik motoru (`physicsWorld.step()`) birkaç kare simüle edilecek.
- Kemiklerin dünya konumlarının başlangıç konumlarından yer çekimi etkisiyle değiştiği doğrulanacak (fizik motorunun kemikleri başarıyla devraldığının kanıtı).

### Manual Verification
- Editör üzerinden bir karaktere Ragdoll state atandığında, mesh'in doğal bir şekilde (eklemlerinden kırılarak) yere düştüğü gözlemlenecek.
