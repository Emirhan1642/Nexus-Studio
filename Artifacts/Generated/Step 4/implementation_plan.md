# Aşama 5: Fizik (Jolt Physics Entegrasyonu)

Bu aşamada, motorumuzun fizik altyapısını oluşturmak için `Jolt Physics` kütüphanesini projeye dahil edeceğiz. Faz 1-4'te oluşturduğumuz DataModel (Ağaç yapısı), Render (Görselleştirme) ve Scripting (Luau) sistemlerinin arasına **Fizik Sistemini** bir köprü olarak yerleştireceğiz. Hedefimiz `Part` nesnelerinin yerçekimiyle düşmesi, çarpışması ve `Touched` olayının tetiklenmesidir.

## User Review Required

> [!IMPORTANT]
> - **Jolt Physics Kütüphanesi:** Jolt Physics, `FetchContent` ile `ThirdParty/CMakeLists.txt` içerisine eklenecektir. Bu kütüphane büyük olduğu için projenin derlenme süresini (özellikle ilk derlemede) uzatabilir.
> - **Stud vs Metre Ölçeği:** Motorumuz içindeki birimler (Roblox'a sadık kalarak) **Stud** olacaktır (1 Stud ≈ 0.28m veya ~3.57 Stud = 1m). Fizik motoru metrik sistemle çalıştığı için, DataModel'den fizik motoruna veri giderken ve gelirken şeffaf bir dönüştürücü (Converter) kullanılacaktır.

## Open Questions

> [!WARNING]
> 1. Jolt Physics çoklu iş parçacığı (multi-threading) desteği olan bir motor. Çarpışma olayları (ContactListener) farklı thread'lerden gelebileceği için, Luau script'lerini güvenle tetikleyebilmek adına "Thread-Safe Event Queue" mantığını kuracağız. Bu yöntem size uygun mu?
> 2. `WeldConstraint` gibi Constraint sınıflarını (Part'ları birbirine yapıştıran sınıflar) hemen bu aşamanın başında prototipleyelim mi, yoksa sadece yerçekimi ve düz çarpışma ile başlayıp ilerleyelim mi? (Planımda sadece temel çarpışma, yerçekimi ve Touched event'ine odaklanmak var).

## Proposed Changes

### 1. CMake Yapılandırması
#### [MODIFY] [ThirdParty/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/ThirdParty/CMakeLists.txt)
- `FetchContent_Declare` ile `jrouwe/JoltPhysics` deposu projeye eklenecek.
#### [MODIFY] [Engine/Core/CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/CMakeLists.txt)
- `EngineCore` hedefine `JoltPhysics` kütüphanesi linker (bağlayıcı) bağımlılığı olarak eklenecek.

### 2. Fizik Sistemi (Physics System)
#### [NEW] [Engine/Physics/PhysicsWorld.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Physics/PhysicsWorld.h)
- Jolt Physics motorunun başlatılması, Memory allocator'ların atanması, Katmanların (Static/Dynamic) belirlenmesi.
- Sabit zaman adımlı (Fixed Timestep) `step(deltaTime)` metodunun yazılması.
#### [NEW] [Engine/Physics/PhysicsConversions.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Physics/PhysicsConversions.h)
- Jolt (Metre/Quat) ve Nexus (Stud/Matrix) arası veri dönüştürme fonksiyonları.
#### [NEW] [Engine/Physics/ContactListenerImpl.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Physics/ContactListenerImpl.h)
- Çarpışmaları dinleyecek ve thread-safe bir kuyruğa atacak özel sınıf.

### 3. DataModel ⟷ Jolt Bağlantısı
#### [MODIFY] [Engine/Core/DataModel/Part.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.h)
#### [MODIFY] [Engine/Core/DataModel/Part.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.cpp)
- Part, DataModel'e (Workspace) eklendiğinde karşılık gelen bir Jolt `BodyID` oluşturacak.
- `Anchored` özelliği eklenecek (Static vs Dynamic Body).
- `Part::setPosition` fonksiyonu, eğer fizik objesi mevcutsa Jolt tarafını da güncelleyecek (Tek Yönlü Senkronizasyon).

### 4. Ana Döngü (Main Loop) ve Senkronizasyon
#### [MODIFY] [Editor/Main.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Editor/Main.cpp)
- Ana döngü içerisinde `PhysicsWorld::step()` fonksiyonu çağrılacak.
- Fizik adımı bittikten sonra Jolt motorunda hareket eden tüm objelerin yeni pozisyonları alınıp DataModel'deki `Part` nesnelerine yazılacak (Çift Yönlü Senkronizasyon).
- Çarpışma kuyruğundaki (Event Queue) olaylar işlenip `Touched` sinyali (Signal) ateşlenecek (Luau'ya haber vermek için).

## Verification Plan

### Otomatik Testler
- Jolt kütüphanesinin başarıyla indirilip derlendiği test edilecek.

### Manuel Doğrulama
1. **Düşme Testi:** Arayüz (Editor) açıldığında `Anchored = false` olan `MyCube1` objesinin aşağı doğru düştüğü gözlemlenecek.
2. **Çarpışma Testi:** Düşen küpün, `Anchored = true` olan altındaki başka bir küpe (Zemin) çarptığında durduğu görülecek.
3. **Senkronizasyon Testi:** Viewport üzerinde ImGuizmo ile hareket ettirilen fiziksel bir objenin Jolt sisteminde de doğru pozisyona gittiği doğrulanacak.
