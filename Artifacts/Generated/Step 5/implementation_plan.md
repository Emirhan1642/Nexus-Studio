# Aşama 5: Karakter Kontrolcüsü (Humanoid) - İmplementasyon Planı

Karakter kontrolcüsü (Humanoid), Jolt'un `CharacterVirtual` sistemini, DataModel'i ve Scripting'i birleştiren karmaşık bir gameplay bileşenidir. Bu plan, `Karakter_Kontrolcusu_Humanoid.md` dokümanındaki gereksinimleri koda dökmeyi amaçlar.

## Önerilen Değişiklikler

### 1. Fizik Filtrelerinin Dışa Açılması (Engine/Physics)
`CharacterVirtual::ExtendedUpdate` fonksiyonu, Jolt'un filtre sınıflarına ve `TempAllocator`'a ihtiyaç duyar.
- **MODIFY** `Engine/Physics/PhysicsWorld.h`: 
  - `BPLayerInterfaceImpl`, `ObjectVsBroadPhaseLayerFilterImpl`, `ObjectLayerPairFilterImpl` ve `TempAllocator` nesnelerini `public` erişime veya getter metodlarına açacağız.
  - Ragdoll için yeni fizik katmanı (`Layers::RAGDOLL` vs.) ekleyeceğiz.

### 2. Humanoid Sınıfı ve State Machine (Engine/Core/DataModel)
- **NEW** `Engine/Core/DataModel/Humanoid.h` & `.cpp`:
  - `Instance` sınıfından türeyecek.
  - Özellikler: `walkSpeed`, `jumpPower`, `health`, `maxHealth`, `state` (Enum).
  - Metodlar: `MoveTo`, `Jump`, `enterRagdoll`.
  - Jolt'tan `JPH::CharacterVirtual` barındıracak.
  - `ClassBuilder<Humanoid>` ve `EnumRegistry` (HumanoidState) ile Luau'ya açılacak.
- **NEW** `Engine/Core/DataModel/HumanoidStateMachine.h`:
  - `Idle`, `Walking`, `Jumping`, `Falling`, `Landed`, `Ragdoll` geçişlerini yönetecek.

### 3. Ragdoll (Fiziksel Düşüş) Sistemi
Kullanıcı talebi üzerine tam olarak dokümanda anlatıldığı gibi eklenecektir.
- Karakter öldüğünde (`health <= 0` vb.) veya bilerek tetiklendiğinde `CharacterVirtual` kapatılır.
- Vücudun parçaları (gövde, bacaklar, kollar) için `JPH::Body` yaratılır, `JPH::Constraint` (eklem) ile birbirine bağlanır ve tam bir fiziksel zincir oluşturulur.

### 4. Client-Side Prediction (HumanoidPredictor) ve GNS Entegrasyonu
Faz 6'daki `ClientPredictor` mantığı doğrudan Humanoid için tam olarak uygulanacaktır.
- **NEW** `Engine/Networking/Prediction/HumanoidPredictor.h` & `.cpp`:
  - `HumanoidInputCommand` (ileriye/sağa, zıplama isteği) tanımlanıp Protobuf / GNS üzerinden sunucuya gönderilir.
  - Local (Yerel) Humanoid, komutu bekletmeden hemen `CharacterVirtual` üzerinden çalıştırır ve tahminde bulunur.
  - Sunucudan `serverPos` ve `serverVelocity` içeren onaylı snapshot geldiğinde, bekleyen (onaylanmamış) komutların üstünden Replay (geri sarma ve yeniden oynatma) yapılarak düzeltilir.

## Doğrulama Planı
- **Derleme:** CMake üzerinden Jolt Character kütüphaneleri ve GNS ağ mesajlarıyla hatasız derlenmesi.
- **Birim Testleri:** `Humanoid` sınıfının özelliklerinin ve metodlarının reflection sistemine başarıyla kaydedildiğini doğrulayan C++ testleri eklenecek.
- **Ağ/Prediction Testi:** GNS üzerinden InputCommand gönderiminin başarılı olduğu gözlemlenecek.
