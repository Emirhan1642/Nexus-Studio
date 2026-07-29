# Humanoid Karakter Kontrolcüsü Entegrasyonu

Bu aşamada Roblox benzeri bir `Humanoid` sınıfı oluşturarak, karakterlerin fizik kurallarına uygun ama aynı zamanda oynanabilir (basamak çıkma, eğim çıkma vb.) şekilde hareket etmesini sağlayacağız. Jolt'un `CharacterVirtual` sistemini kullanarak tam sanal bir karakter kontrolcüsü inşa edeceğiz.

## Hedefler
- `Humanoid` sınıfının (ve `HumanoidState` durum makinesinin) DataModel altyapısına eklenmesi.
- `WalkSpeed`, `JumpPower` gibi değerlerin Reflection API üzerinden Scripting/Networking'e açık hale getirilmesi.
- `CharacterVirtual` (Jolt) kullanılarak fiziksel olarak stabil bir karakter hareketi (MoveTo ve Jump) sağlanması.
- Karakterlerin her frame Jolt fizik döngüsünde güncellenebilmesi.

## Önerilen Değişiklikler

### 1. `Engine/Core/DataModel/Humanoid.h` ve `.cpp`
Bu dosyalarda `Humanoid` bileşenini tasarlayacağız:
- `HumanoidState` enum yapısı (Idle, Walking, Jumping, Falling vb.).
- `Instance` sınıfından türeyen `Humanoid`.
- Özellikler: `WalkSpeed` (16.0f), `JumpPower` (50.0f).
- `ObjectRef` özelliği: `RootPart` (Karakterin fiziksel kapsül şeklini temsil eden Part).
- `moveTo(Vector3 direction)` ve `jump()` fonksiyonları.
- Jolt tabanlı `CharacterVirtual` nesnesi barındırılacak ve `RootPart` set edildiğinde/değiştiğinde Jolt dünyasında oluşturulacak.

### 2. `Engine/Physics/PhysicsWorld` Güncellemesi
`CharacterVirtual` nesneleri, Jolt'un olağan (rigid body) çözümleyicisinden bağımsız olarak, özel `ExtendedUpdate` çağrılarıyla güncellenmelidir.
- `PhysicsWorld` içine, oyundaki aktif `Humanoid`'leri tutan bir liste eklenecek (`registerHumanoid` / `unregisterHumanoid`).
- `PhysicsWorld::step(float deltaTime)` fonksiyonunda, `physicsSystem.Update` öncesi (veya hemen sonrası) tüm Humanoid'lerin `ExtendedUpdate` fonksiyonu çağrılacak.

### 3. Jolt CharacterVirtual Entegrasyonu (Humanoid::update)
`Humanoid` için `update(deltaTime)` metodu yazılacak. Bu metot:
- Yere basıp basmadığını (`GetGroundState`) kontrol edip `HumanoidState`'i (Idle, Walking, Falling vb.) güncelleyecek.
- Eğer yürüme komutu verildiyse (hedef `WalkSpeed` ve yön), mevcut dikey hızı (gravity) koruyarak yatayda `CharacterVirtual`'ın hızını ayarlayacak.
- `ExtendedUpdate` çağrısıyla basamak (step-up) ve yokuş kontrollerini yapacak.
- Karakterin yeni sanal pozisyonunu, `RootPart`'ın (Instance) pozisyonuna eşitleyecek.

### 4. Unit Testler
- Yeni bir `HumanoidTest` (veya mevcut fizik testine bir bölüm) eklenecek.
- Bir zemin (`Part`) ve üzerinde bir `Humanoid` oluşturulacak.
- `MoveTo` çağrılarak karakterin fiziksel olarak ilerlediği (zemin üzerinde yürüdüğü) test edilecek.

## User Review Required

> [!IMPORTANT]
> Jolt'ta bir `CharacterVirtual` oluşturmak için bir fiziksel `Shape`'e (Kapsül, Silindir vb.) ihtiyacımız var. Planıma göre, `Humanoid` sınıfına bir `RootPart` atandığında, bu `RootPart`'ın `size` (boyut) parametresine bakarak **otomatik olarak bir Kapsül (Capsule) şekli** yaratacağız. (Normalde `Part` bir küp bile olsa, Humanoid eklendiğinde kapsül olarak davranması, merdiven çıkabilmesi için şarttır). 
> 
> Bu davranış oyun motorunun tasarımı açısından uygun mudur?

## Doğrulama Planı (Verification Plan)
- Derleme: CMake üzerinden sorunsuz build alınması.
- Unit Test: `NexusStudioTests` üzerinden Jolt fizik simülasyonunda karakterin başarıyla yaratılıp, zemin üzerinde düşmeden ve yerçekimine maruz kalarak `WalkSpeed` oranında ilerlediğinin assert'ler ile doğrulanması.
