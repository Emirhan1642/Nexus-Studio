# Aşama 5: Karakter Kontrolcüsü (Humanoid) - Görev Listesi

- `[x]` **Grup 1: Fizik Filtrelerinin Dışa Açılması**
  - `[x]` `Engine/Physics/PhysicsWorld.h` içinde Jolt filtre ve tahsis edici (allocator) sınıflarının getter metodlarını ekle.
  - `[x]` Ragdoll çarpışmaları için Jolt `Layers::RAGDOLL` tanımlamalarını ekle.

- `[x]` **Grup 2: Temel Humanoid Sınıfı**
  - `[x]` `Engine/Core/DataModel/Humanoid.h` ve `.cpp` oluştur (Instance türevli).
  - `[x]` Humanoid özellikleri (`walkSpeed`, `jumpPower`, vb.) ve metodlarını ekle.
  - `[x]` Jolt `CharacterVirtual` entegrasyonunu yap (`ExtendedUpdate` çağrısı dahil).
  - `[x]` `ClassBuilder` ve `EnumRegistry` (HumanoidState) kullanarak reflection kaydını tamamla.

- `[x]` **Grup 3: Humanoid State Machine**
  - `[x]` `Engine/Core/DataModel/HumanoidStateMachine.h` oluştur.
  - `[x]` Zemin durumuna (InAir/OnGround) ve inputa bağlı olarak Idle/Walking/Jumping/Falling geçiş mantığını yaz.
  - `[x]` State değiştiğinde Signal ateşleme yapısını kur.

- `[x]` **Grup 4: Ragdoll Sistemi (Fiziksel Düşüş)**
  - `[x]` `Humanoid::enterRagdoll()` metodunu doldur.
  - `[x]` Karakter parçaları (gövde, kol, bacak) için `JPH::Body` zincirini ve `JPH::Constraint` (eklem) bağlantılarını oluştur.
  - `[x]` Ragdoll aktifken `CharacterVirtual`'ı kapat.

- `[x]` **Grup 5: Client-Side Prediction ve Networking**
  - `[x]` `Engine/Networking/Prediction/HumanoidPredictor.h` ve `.cpp` oluştur.
  - `[x]` `HumanoidInputCommand` yapısını kur ve GNS mesaj tipleri arasına ekle.
  - `[x]` Yerel girdi -> anında hareket (Prediction) -> Sunucu Onayı -> Rollback/Replay döngüsünü uygula.

- `[x]` **Grup 6: Entegrasyon ve Test**
  - `[x]` CMake `CMakeLists.txt` dosyalarına yeni sınıfları ekle.
  - `[x]` Projeyi baştan derle ve hata ayıkla (Debugging).
  - `[x]` `NexusStudioTests.exe` için `Humanoid` odaklı bir test yaz (ClassBuilder doğrulaması vs.).
