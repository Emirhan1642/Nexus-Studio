# Aşama 5: Fizik (Jolt Physics Entegrasyonu) - Görev Listesi

- `[x]` 1. CMake Yapılandırması
  - `[x]` `ThirdParty/CMakeLists.txt` dosyasına JoltPhysics ekle
  - `[x]` `Engine/Core/CMakeLists.txt` dosyasına bağımlılığı ekle
- `[x]` 2. Fizik Sistemi (Physics System)
  - `[x]` `Engine/Physics/PhysicsWorld.h` ve `.cpp` oluştur
  - `[x]` `Engine/Physics/PhysicsConversions.h` (Stud/Metre) oluştur
  - `[x]` `Engine/Physics/ContactListenerImpl.h` (Çarpışma kuyruğu) oluştur
- `[x]` 3. DataModel ⟷ Jolt Bağlantısı
  - `[x]` `Part` sınıfına `Anchored` özelliğini ekle ve Reflection kaydını yap
  - `[x]` `Part` Workspace'e eklendiğinde Jolt `BodyID` oluştur
  - `[x]` `Part::setPosition` içinde Jolt tarafını (tek yönlü) güncelle
- `[x]` 4. Ana Döngü (Main Loop) ve Senkronizasyon
  - `[x]` `Editor/Main.cpp` içinde `PhysicsWorld::step()` fonksiyonunu bağla
  - `[x]` Hareket eden body'lerin pozisyonlarını `Part` nesnelerine yaz (çift yönlü senkronizasyon)
  - `[x]` Çarpışma kuyruğunu işleyip `Touched` event'ini tetikle
- `[x]` 5. Derleme ve Test
  - `[x]` Projeyi baştan derle
  - `[ ]` Çalışma anında Jolt'un küpleri düşürdüğünü doğrula
