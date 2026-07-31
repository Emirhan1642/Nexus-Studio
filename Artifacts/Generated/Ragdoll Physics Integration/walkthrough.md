# Inverse Kinematics (IK) — Implementation Walkthrough

## Genel Bakış

Bu belge, Nexus Studio motoruna eklenen **Inverse Kinematics (IK)** sisteminin teknik detaylarını açıklamaktadır. IK, bir karakterin eliyle bir kapı koluna uzanması veya ayağının eğimli bir zemine adaptasyonu gibi durumlarda, eklem zincirini belirli bir hedefe otomatik olarak yönlendiren bir animasyon tekniğidir.

## Phase 16: Ragdoll Physics Integration (Completed)
Fully transitioned `Humanoid` to skeletal-based physics using `PhysicsAsset`.

### 1. PhysicsAsset Implementation
- Created `PhysicsAsset` to define per-bone collision volumes (`PhysicsBoneShape` with radius, halfHeight, localOffset).
- Added `physicsAsset` property to `Humanoid`.

### 2. Enter/Exit Ragdoll
- `enterRagdoll`: Iterates over the `PhysicsAsset` and `Skeleton` to spawn `JPH::Body` instances using `JPH::CapsuleShape`.
- Bone physical bodies are linked hierarchically using `JPH::PointConstraint` or `JPH::ConeConstraint`.
- `exitRagdoll`: Cleans up and destroys all ragdoll bodies and constraints from the physics world.

### 3. Ragdoll Simulation Update
- Updated `Humanoid::update()` logic:
  - When in `Ragdoll` state, standard character virtual physics are suspended.
  - Transforms are read directly from the simulated Jolt bodies and converted back into `Math::Matrix4` bone matrices for GPU skinning.

### 4. Verification
- `HumanoidTest.RagdollSimulation` verifies that entering Ragdoll spawns the physical bodies, simulates gravity correctly causing the bones to fall, and reads back accurate world space positions.

---

## Mimari

IK sistemi **bileşen tabanlı (component-based)** bir yaklaşımla tasarlanmıştır; Roblox'taki `IKControl` bileşenine bilinçli olarak benzetilmiştir.

```
DataModel
  └── Part (HumanoidRootPart)
        └── Humanoid          ← Animasyon döngüsünü yönetir
              └── IKControl   ← IK hedefini ve zincirini tanımlar
```

`IKControl`, bir `Instance`'ın child'ı olarak eklenir. `Humanoid`, her güncelleme adımında children'larını tarayarak `IKControl` örneklerini tespit eder ve uygulamadan önce skeleton'ı günceller.

---

## Eklenen / Değiştirilen Dosyalar

### [NEW] [IKControl.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/IKControl.h)

`IKControl` sınıfının header dosyası. `Instance`'dan türer ve şu property'leri barındırır:

| Property | Tür | Açıklama |
|---|---|---|
| `endEffector` | `std::string` | Hedeflenecek bone'un adı (örn. `"Hand_R"`) |
| `targetPosition` | `Vector3` | IK zincirinin ulaşmaya çalışacağı dünya koordinatı |
| `poleVector` | `Vector3` | Orta eklemi yönlendiren pole vektörü (varsayılan: `+Z`) |
| `weight` | `float` | IK etkisinin ağırlığı `[0.0 – 1.0]` |

```cpp
class IKControl : public Instance {
public:
    std::string  endEffector;
    Math::Vector3 targetPosition;
    Math::Vector3 poleVector = {0.0f, 0.0f, 1.0f};
    float         weight = 1.0f;

    void apply(Animation::Skeleton& skeleton,
               std::vector<Math::Matrix4>& localPose,
               const std::vector<Math::Matrix4>& worldPose);
};
```

---

### [NEW] [IKControl.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/IKControl.cpp)

IK çözücüsünün gövdesi. **CCD (Cyclic Coordinate Descent)** algoritması kullanılmaktadır.

#### Algoritma Akışı

```
apply() çağrıldığında:
  1. endEffector bone'unu skeleton içinde bul  → endIndex
  2. Ebeveyn zincirini takip et               → midIndex, rootIndex
  3. Hedefin maksimum erişim mesafesini kontrol et (over-stretch engeli)
  4. CCD iterasyonu (2 tur):
     a. Mid bone'u hedef yönüne döndür
     b. World pose'u yeniden hesapla (partial recompute)
     c. Root bone'u hedef yönüne döndür
  5. Rotasyonları weight ile lerp/slerp et → localPose'a yaz
```

#### Over-stretch Koruması

```cpp
float maxDist = (midPos - rootPos).length() + (endPos - midPos).length() - 0.001f;
if (targetDir.length() > maxDist) {
    targetDir.normalize();
    target = rootPos + targetDir * maxDist;
}
```

Eklem zincirinin tam uzunluğunu aşan hedefler için hedef pozisyonu sıkıştırılır; böylece kemikler fizik dışı bir şekilde uzamaz.

#### World → Local Uzay Dönüşümü

CCD her bone'u dünya uzayında döndürdüğü için, bu rotasyonu bone'un parent'ının local uzayına dönüştürmek gerekir:

```cpp
Math::Quaternion localRotDelta = parentRot.inverse() * rotWorld * parentRot;
Math::Quaternion finalRot      = localRotDelta * boneRot;
boneRot = boneRot.slerp(finalRot, weight);   // weight ile blend
localPose[boneIndex] = Matrix4::fromTRS(trans, boneRot, scale);
```

---

### [MODIFY] [Humanoid.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/Humanoid.cpp)

Animasyon değerlendirmesinden sonra, dünya transformları hesaplanmadan önce IK kancası eklendi:

```cpp
// Evaluate skeletal animation
std::vector<Matrix4> localPose  = animationPlayer.evaluate(skeleton, deltaTime);
std::vector<Matrix4> worldPose  = skeleton.computeWorldTransforms(localPose);

// IK Integration Hook
if (ikEnabled) {
    bool ikApplied = false;
    for (auto& child : getChildren()) {
        if (auto ik = std::dynamic_pointer_cast<IKControl>(child)) {
            ik->apply(skeleton, localPose, worldPose);
            ikApplied = true;
        }
    }
    if (ikApplied) {
        // localPose değiştiyse dünya transformlarını yeniden hesapla
        worldPose = skeleton.computeWorldTransforms(localPose);
    }
}

// Skinning matrislerini hesapla
for (size_t i = 0; i < skeleton.bones.size(); ++i)
    finalBoneTransforms[i] = worldPose[i] * skeleton.bones[i].inverseBindPoseWorldTransform;
```

Bu sıra kritiktir:
1. Animasyon oynatıcı local pose'u değerlendirir
2. IK **local pose'u modifiye eder** (world değil, doğrudan lokal uzayda)  
3. World transformlar IK sonrası yeniden hesaplanır
4. GPU'ya gönderilecek skinning matrisleri üretilir

---

### [MODIFY] [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/DataModel/Humanoid.h)

`IKControl.h` include'u eklendi.

---

### [MODIFY] [Engine/Core/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus Studio/Engine/Core/CMakeLists.txt)

`IKControl.cpp` ve `IKControl.h` dosyaları `EngineCore` hedefine eklendi.

---

## Reflection Kaydı

`IKControl`, engine'in reflection sistemi üzerinden `createInstance("IKControl")` çağrısıyla oluşturulabilir.

> [!IMPORTANT]
> **Linker Stripping Sorunu:** C++ statik kütüphanelerinde, yalnızca static değişken içeren bir `.cpp` dosyası linker tarafından tamamen atılabilir. `IKControl.cpp`'deki kayıt kodu başlangıçta kendi TU'sunda yer alıyordu ve `createInstance` her zaman `nullptr` döndürüyordu. Bu sorun, kaydın zaten kesinlikle linklenen `Humanoid.cpp`'deki `registerHumanoid()` fonksiyonuna taşınmasıyla çözüldü.

```cpp
// Humanoid.cpp → registerHumanoid() içinde
Engine::Reflection::ClassBuilder<IKControl>("IKControl")
    .base("Instance")
    .property("EndEffector",    &IKControl::endEffector)
    .property("TargetPosition", &IKControl::targetPosition)
    .property("PoleVector",     &IKControl::poleVector)
    .property("Weight",         &IKControl::weight);
```

---

## Kullanım Örneği (Luau / C++)

### C++ (Test ortamı)
```cpp
auto humanoid  = std::make_shared<Humanoid>();
humanoid->setParent(rootPart);

auto ik = std::static_pointer_cast<IKControl>(createInstance("IKControl"));
ik->endEffector   = "Hand_R";
ik->targetPosition = Vector3(1.5f, 1.2f, 0.5f);
ik->weight         = 1.0f;
ik->setParent(humanoid);   // Bu kadar — her fizik adımında otomatik uygulanır
```

### Luau (Script tarafı)
```lua
local humanoid = character:FindFirstChildOfClass("Humanoid")

local ik = Instance.new("IKControl")
ik.EndEffector   = "Hand_R"
ik.TargetPosition = Vector3.new(1.5, 1.2, 0.5)
ik.Weight         = 1.0
ik.Parent         = humanoid
```

---

## Test Sonuçları

```
[==========] Running 15 tests from 5 test suites.
[  PASSED  ] 15 tests.

HumanoidTest.BasicMovement          → OK (5 ms)
HumanoidTest.IKControlApplication   → OK (0 ms)
```

`IKControlApplication` testi şunları doğrular:
- `IKControl` TypeRegistry'de kayıtlı ve `createInstance` ile oluşturulabilir
- `Humanoid`'e child olarak eklenebilir
- Fizik adımı sırasında crash olmadan çalışır (skeleton boşken `apply()` erken döner)
- `weight`, `endEffector` ve parent hiyerarşisi doğrudur

---

## Sınırlılıklar ve Gelecek İyileştirmeler

| Konu | Durum | Not |
|---|---|---|
| Algoritma | CCD (2 iterasyon, 2 bone) | FABRIK daha hızlı yakınsama sağlar |
| Pole vektörü | Tanımlı ama henüz CCD'ye entegre değil | Diz/dirsek yönlendirmesi için gerekli |
| Çoklu zincir | Desteklenmiyor | Birden fazla `IKControl` child eklenerek kısmen mümkün |
| Editor UI | Yok | Properties panelinde IKControl görünür olmalı |
| Performans | Her adımda `computeWorldTransforms` çağrısı | Dirty-flag optimizasyonu eklenebilir |

---

## Bağımlılıklar

- `Quaternion::slerp`, `Quaternion::inverse`, `Quaternion::fromAxisAngle`
- `Matrix4::decompose`, `Matrix4::fromTRS`, `Matrix4::getTranslation`
- `Skeleton::findBoneIndex`, `Skeleton::computeWorldTransforms`
- `Reflection::ClassBuilder<T>` — reflection kaydı

