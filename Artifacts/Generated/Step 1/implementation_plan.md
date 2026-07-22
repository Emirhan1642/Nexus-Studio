# Sıfırdan Oyun Motoru - Aşama 1 (Faz 0 & Faz 1)

Bu plan, `Faz0_Faz1_Teknik_Detay.md` ve `Faz1_Derinlestirme.md` dokümanlarında belirtilen mimariyi koda dökmek için hazırlanmıştır. Projenin temel CMake iskeletini, bgfx+GLFW ile pencere açılışını ve çok kritik olan Core Reflection (Yansıma) ve DataModel (Instance) sistemlerini inşa edeceğiz.

## User Review Required / Open Questions

> [!NOTE]
> Dokümanlarda fark ettiğim bazı mantıksal eksiklikleri ve önerilerimi aşağıda listeledim. Lütfen bu konularda onay verin veya tercihinizi belirtin:

1. **Kök Dizin:** Projenin C++ dosyalarını doğrudan `c:\Users\Emirhan\Desktop\Emirhan\Projects\Nexus Studio` dizinine mi (burayı kök kabul ederek) kuralım, yoksa bu dizinin içinde `GameEngine` adında bir alt klasör mü oluşturalım? (Ben aşağıda doğrudan Nexus Studio dizinini kök alarak planladım).
2. **GLFW Eksikliği:** Dokümandaki `ThirdParty` CMake örneğinde `bgfx`, `Jolt` ve `Luau` çekiliyor ancak Faz 0 kodunda `GLFW` kullanılmasına rağmen CMake'e eklenmemiş. Pencere oluşturmak için GLFW'yu da `FetchContent` ile projeye dahil edeceğim.
3. **Test Kütüphanesi:** Dokümanda `enable_testing()` denilmiş ama hangi kütüphanenin kullanılacağı belirtilmemiş. Endüstri standardı olan **GoogleTest (gtest)**'i projeye dahil edip Faz 1'in "Definition of Done" listesindeki Reflection testlerini bununla yazmayı öneriyorum. Uygun mudur?
4. **Vector3:** `Part` sınıfında `Vector3` kullanılıyor ancak bu tanımlanmamış. Şimdilik `float x, y, z;` içeren çok basit bir `Vector3` struct'ı oluşturacağım. İleride (Faz 2 veya 5'te) glm veya Jolt'un vektör sınıflarıyla değiştirilebilir.

## Proposed Changes

Aşağıdaki klasör ve dosya yapısı oluşturulacaktır:

### 1. Kök CMake ve Proje İskeleti
- `[NEW]` `CMakeLists.txt` (Kök dizin - C++20 ayarları, alt klasörleri ekleme)
- `[NEW]` `CMakePresets.json` (Geliştirici dostu build presetleri)

### 2. ThirdParty (Hazır Kütüphaneler)
- `[NEW]` `ThirdParty/CMakeLists.txt`
  - `bgfx` (bkaradzic/bgfx.cmake üzerinden)
  - `glfw` (Pencere yönetimi)
  - `googletest` (Birim testler için)
  - *(Jolt ve Luau Faz 1'de kullanılmayacağı için indirme sürelerini uzatmamak adına şu an eklenmeyecek, kendi fazlarında eklenecek).*

### 3. Engine Modülü
- `[NEW]` `Engine/CMakeLists.txt` (Statik library)
- `[NEW]` `Engine/Core/CMakeLists.txt`
- **Reflection Sistemi:**
  - `[NEW]` `Engine/Core/Reflection/TypeRegistry.h` (Kayıt tablosu, deferBaseClass ve finalize mantığı)
  - `[NEW]` `Engine/Core/Reflection/TypeRegistry.cpp`
  - `[NEW]` `Engine/Core/Reflection/ClassBuilder.h` (Fluent API, property, enumProperty, arrayProperty, objectProperty, method)
  - `[NEW]` `Engine/Core/Reflection/EnumRegistry.h`
  - `[NEW]` `Engine/Core/Reflection/EnumRegistry.cpp`
- **DataModel Sistemi:**
  - `[NEW]` `Engine/Core/DataModel/Instance.h` (İntrusive reference counting, weak_ptr parent, shared_ptr children)
  - `[NEW]` `Engine/Core/DataModel/Instance.cpp` (Destroy metodu)
  - `[NEW]` `Engine/Core/DataModel/DataModel.h` (Root instance)
  - `[NEW]` `Engine/Core/DataModel/Part.h` (Temel 3D obje)
  - `[NEW]` `Engine/Core/DataModel/Part.cpp` (Reflection registration, Material enum)
- **Math:**
  - `[NEW]` `Engine/Core/Math/Vector3.h` (Basit x,y,z struct'ı)

### 4. Editor (Faz 0 - Boş Pencere)
- `[NEW]` `Editor/CMakeLists.txt` (Executable)
- `[NEW]` `Editor/Main.cpp` (GLFW başlatma, bgfx başlatma, TypeRegistry::finalize() çağrısı ve ana render döngüsü)

### 5. Tests (Faz 1 Doğrulaması)
- `[NEW]` `Tests/CMakeLists.txt`
- `[NEW]` `Tests/ReflectionTests.cpp` (IsA, property set/get, weak_ptr kontrolü, enum testleri)

## Verification Plan

### Automated Tests
GoogleTest üzerinden şu testler çalıştırılarak Faz 1'in "Definition of Done" listesi doğrulanacak:
- `ctest -C Debug -V`
- Hiyerarşi (Parent/Child) ve Circular Reference memory sızıntı testleri
- `IsA("Instance")` kalıtım zinciri testleri
- Reflection üzerinden obje üretimi (`createInstance`)
- Enum, Array ve ObjectRef property'lerinin set/get testleri

### Manual Verification
- `Editor` executable'ı çalıştırıldığında boş bir bgfx penceresinin sorunsuz açılıp kapandığı gözlemlenecek.
