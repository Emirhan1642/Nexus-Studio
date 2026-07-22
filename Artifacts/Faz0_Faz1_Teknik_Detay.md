# Faz 0 & Faz 1 — Teknik Derinlemesine İnceleme
## Proje İskeleti, Reflection Sistemi ve DataModel Mimarisi

Bu doküman, ana Implementation Plan'daki Faz 0 (Temel Altyapı) ve Faz 1 (Çekirdek Nesne Sistemi) için somut kod seviyesinde tasarım kararlarını içerir. Buradaki kararlar projenin geri kalanının üzerine oturacağı temeldir — bu yüzden en çok zaman ayrılması gereken kısım burasıdır.

---

## Bölüm A — Faz 0: Proje İskeleti

### A.1 Klasör yapısı ile CMake ilişkisi

Her klasörün kendi `CMakeLists.txt` dosyası olacak, kök dizin bunları toplayacak. Bu yaklaşımın adı "modüler CMake" — her modül bağımsız derlenebilir ve test edilebilir hale gelir.

```
GameEngine/
├── CMakeLists.txt                  # Kök: alt projeleri toplar
├── CMakePresets.json               # Debug/Release/Platform presetleri
│
├── Engine/
│   ├── CMakeLists.txt              # Engine statik kütüphane olarak derlenir
│   ├── Core/CMakeLists.txt
│   ├── Renderer/CMakeLists.txt
│   ├── Physics/CMakeLists.txt
│   └── ...
│
├── Editor/
│   └── CMakeLists.txt              # Executable, Engine'e bağımlı
│
├── Runtime/
│   └── CMakeLists.txt              # Executable, Engine'e bağımlı (Editor'suz)
│
└── ThirdParty/
    └── CMakeLists.txt              # bgfx, Jolt, Luau vb. FetchContent ile çekilir
```

### A.2 Kök CMakeLists.txt (somut örnek)

```cmake
cmake_minimum_required(VERSION 3.24)
project(GameEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tüm alt projeler bu klasörden binary üretir (build klasörü kirlenmesin diye)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory(ThirdParty)
add_subdirectory(Engine)
add_subdirectory(Editor)
add_subdirectory(Runtime)

# Opsiyonel: test altyapısı (Faz 0'da eklenmesi şiddetle önerilir)
enable_testing()
add_subdirectory(Tests)
```

### A.3 Üçüncü parti kütüphaneleri yönetme stratejisi

**Karar: Git submodule değil, CMake `FetchContent` kullanılacak.**

Gerekçe: Submodule'ler ekip büyüdükçe "unuttum güncellemeyi" sorunlarına yol açar. FetchContent, hangi commit'e kilitlendiğini doğrudan CMake dosyasında gösterir — bu da versiyon kontrolü açısından daha şeffaftır.

```cmake
# ThirdParty/CMakeLists.txt
include(FetchContent)

FetchContent_Declare(
    bgfx
    GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
    GIT_TAG        <belirli-bir-commit-hash>   # Asla "main" branch'ine kilitleme
)
FetchContent_MakeAvailable(bgfx)

FetchContent_Declare(
    JoltPhysics
    GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
    GIT_TAG        <belirli-bir-commit-hash>
    SOURCE_SUBDIR  Build
)
FetchContent_MakeAvailable(JoltPhysics)

FetchContent_Declare(
    luau
    GIT_REPOSITORY https://github.com/luau-lang/luau.git
    GIT_TAG        <belirli-bir-commit-hash>
)
FetchContent_MakeAvailable(luau)
```

**Önemli uyarı:** `GIT_TAG` için asla `main` veya `master` yazma. Kütüphane sahibi bir gün breaking change içeren bir commit atarsa, senin projen habersizce bozulur. Her zaman spesifik bir commit hash'i veya release tag'i (`v1.2.3`) kullan.

### A.4 Faz 0'ın somut çıktısı — minimal main.cpp

Bu, Faz 0'ın "definition of done"ıdır — bu kod derlenip çalışmadan Faz 1'e geçilmez:

```cpp
// Editor/Main.cpp
#include <GLFW/glfw3.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

int main() {
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // bgfx kendi API'sini yönetecek
    GLFWwindow* window = glfwCreateWindow(1280, 720, "GameEngine Editor", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }

    bgfx::PlatformData pd{};
    pd.nwh = glfwGetWin32Window(window); // Platforma göre değişir (Win32/Cocoa/X11)

    bgfx::Init init;
    init.platformData = pd;
    init.type = bgfx::RendererType::Count; // Otomatik en iyi API'yi seçer (Vulkan/D3D12/Metal)
    bgfx::init(init);

    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        bgfx::touch(0);
        bgfx::frame();
    }

    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

**Bu kodun anlamı:** Şu an hiçbir "motor" yok — sadece bir pencere açılıp, bgfx'in doğru GPU API'sini (Vulkan/D3D12/Metal) otomatik seçip ekranı temizlemesi sağlanıyor. Faz 0'ın tek amacı bu — geri kalan her şey Faz 1'den itibaren bunun üzerine inşa edilecek.

---

## Bölüm B — Faz 1: Reflection Sistemi

### B.1 Neden reflection'a bu kadar önem veriyoruz?

Reflection olmadan şu senaryo imkânsız hale gelir:

- Editördeki Properties paneli, seçtiğin objenin hangi property'lere sahip olduğunu **runtime'da** bilmeli (compile-time'da değil, çünkü editör her C++ sınıfını "elle" listeleyemez)
- Luau scriptinin içinden `part.Position` yazdığında, bu C++'taki `Part::position` üyesine ulaşabilmeli
- Sahne kaydedilirken, her objenin hangi property'lerinin olduğu otomatik olarak serileştirilebilmeli

Bu üç ihtiyacın **ortak paydası reflection**. Bu yüzden Faz 1'in en kritik parçası budur — hem Editor (Faz 4), hem Scripting (Faz 3), hem Serialization hepsi buna dayanır.

### B.2 Tasarım seçenekleri ve neden bu yaklaşım seçildi

| Yaklaşım | Örnek Kullanan | Artı | Eksi |
|---|---|---|---|
| Macro tabanlı kod üretimi | Unreal (`UCLASS`, `UPROPERTY`) | Çok güçlü, editör entegrasyonu kolay | Ayrı bir derleyici aracı (UHT) gerektirir, kurulumu haftalar sürer |
| Harici code generation aracı | Godot (`.gdextension`) | Temiz ayrım | Build sistemine ekstra karmaşıklık |
| **Template + Fluent API (runtime registration)** | Bizim seçimimiz | Ekstra araç gerektirmez, saf C++20 ile yazılır, hızlı başlanabilir | Compile-time hata yakalama Unreal kadar güçlü değil |

**Karar: Template + Fluent API yaklaşımı.** Gerekçe: Projenin bu aşamasında (Faz 1) ekstra bir kod üretim aracı (Unreal'ın UnrealHeaderTool'u gibi) kurmak, hem geliştirme sürecini yavaşlatır hem de tek/az kişilik bir ekip için gereksiz karmaşıklık katar. İleride proje büyüyünce bu sisteme code-gen katmanı eklenebilir — bu, geriye dönük uyumlu bir genişleme olur.

### B.3 Reflection sisteminin çekirdek API'si

Aşağıdaki kod, bir C++ sınıfının "kendini tanıtmasını" sağlayan sistemin iskeletidir.

```cpp
// Engine/Core/Reflection/TypeRegistry.h

#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <any>
#include <vector>

namespace Engine::Reflection {

// Bir property'nin runtime'da nasıl okunup yazılacağını tanımlar
struct PropertyDescriptor {
    std::string name;
    std::function<std::any(void* instance)> getter;
    std::function<void(void* instance, const std::any& value)> setter;
    std::string typeName; // "Vector3", "float", "string" vb. — editör/script bunu okur
};

// Bir C++ sınıfının reflection metadata'sı
class ClassDescriptor {
public:
    std::string className;
    ClassDescriptor* baseClass = nullptr;   // Kalıtım zinciri için (örn. Part -> Instance)
    std::vector<PropertyDescriptor> properties;
    std::function<void*()> factory;         // "new Part()" işlemini runtime'da tetikler

    const PropertyDescriptor* findProperty(const std::string& name) const {
        for (auto& p : properties)
            if (p.name == name) return &p;
        if (baseClass) return baseClass->findProperty(name); // Kalıtımı da tara
        return nullptr;
    }
};

// Tüm sınıfların kayıtlı olduğu merkezi tablo
class TypeRegistry {
public:
    static TypeRegistry& instance() {
        static TypeRegistry reg;
        return reg;
    }

    ClassDescriptor& registerClass(const std::string& name) {
        auto& desc = classes[name];
        desc.className = name;
        return desc;
    }

    ClassDescriptor* find(const std::string& name) {
        auto it = classes.find(name);
        return it != classes.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, ClassDescriptor> classes;
};

} // namespace Engine::Reflection
```

### B.4 Fluent API — bir sınıfı kayıt etmek nasıl görünüyor?

Yukarıdaki çekirdek üzerine, geliştiricinin gerçekte yazacağı kolay-okunur API şu şekilde kuruluyor:

```cpp
// Engine/Core/Reflection/ClassBuilder.h

#pragma once
#include "TypeRegistry.h"

namespace Engine::Reflection {

template<typename T>
class ClassBuilder {
public:
    explicit ClassBuilder(const std::string& name) {
        descriptor = &TypeRegistry::instance().registerClass(name);
        descriptor->factory = []() -> void* { return new T(); };
    }

    // Property zinciri: .property("Position", &Part::position) şeklinde çağrılır
    template<typename MemberT>
    ClassBuilder& property(const std::string& name, MemberT T::* member) {
        PropertyDescriptor desc;
        desc.name = name;
        desc.typeName = typeid(MemberT).name(); // Basitleştirilmiş; gerçek sistemde type-map kullanılır

        desc.getter = [member](void* instance) -> std::any {
            T* obj = static_cast<T*>(instance);
            return obj->*member;
        };
        desc.setter = [member](void* instance, const std::any& value) {
            T* obj = static_cast<T*>(instance);
            obj->*member = std::any_cast<MemberT>(value);
        };

        descriptor->properties.push_back(std::move(desc));
        return *this;
    }

    ClassBuilder& base(const std::string& baseName) {
        descriptor->baseClass = TypeRegistry::instance().find(baseName);
        return *this;
    }

private:
    ClassDescriptor* descriptor;
};

} // namespace Engine::Reflection
```

### B.5 Kullanım örneği — `Part` sınıfını kayıt etmek

Artık bir motor geliştiricisi yeni bir sınıf eklerken şunu yazar:

```cpp
// Engine/Core/DataModel/Part.h
#include "Engine/Core/Reflection/ClassBuilder.h"
#include "Instance.h"
#include "Vector3.h"

class Part : public Instance {
public:
    Vector3 position;
    Vector3 size{4.0f, 1.0f, 2.0f};
    float transparency = 0.0f;
    bool anchored = false;
};

// Statik bir registration bloğu — program başlarken otomatik çalışır
namespace {
    struct PartReflectionInit {
        PartReflectionInit() {
            using namespace Engine::Reflection;
            ClassBuilder<Part>("Part")
                .base("Instance")
                .property("Position", &Part::position)
                .property("Size", &Part::size)
                .property("Transparency", &Part::transparency)
                .property("Anchored", &Part::anchored);
        }
    } g_partReflectionInit;
}
```

**Bu tasarımın gücü burada görünüyor:** `Part` sınıfını bir kere böyle kayıt ettikten sonra:
- Editördeki Properties paneli otomatik olarak "Position, Size, Transparency, Anchored" alanlarını gösterebilir (kod tekrar yazılmaz)
- Luau binding'i (Faz 3) `part.Position` yazıldığında bu tabloya bakıp doğru getter/setter'ı çağırabilir
- Serialization sistemi sahneyi kaydederken bu tabloyu gezip her property'yi otomatik yazabilir

Üç farklı sistem (Editor, Scripting, Serialization), **aynı kayıt noktasından** besleniyor. Bu yüzden Faz 1'de bu API'nin doğru tasarlanması bu kadar kritik.

---

## Bölüm C — DataModel: Instance Hiyerarşisi ve Bellek Yönetimi

### C.1 Bellek yönetimi stratejisi kararı

**Karar: Intrusive reference counting (Unreal'ın `TSharedPtr`/Roblox'un yaklaşımına benzer), garbage collection değil.**

| Seçenek | Artı | Eksi | Karar |
|---|---|---|---|
| Garbage Collection (Unity/C# tarzı) | Otomatik, geliştirici bellek düşünmez | C++'ta yazması çok zor, durdurma (stop-the-world) gecikmeleri render'ı kesebilir | ❌ |
| Ham pointer + manuel delete | En hızlı | Bug riski çok yüksek, büyük ekipte felaket | ❌ |
| **Intrusive reference counting** | Deterministik, C++'ın RAII'siyle doğal uyum, gecikme yok | Circular reference riski (parent-child'da dikkat gerekir) | ✅ |

Circular reference riski şu şekilde çözülüyor: **Parent → Child ilişkisi güçlü referans (strong ref) olacak, Child → Parent ilişkisi zayıf referans (weak ref) olacak.** Bu, tam olarak Roblox'un ve çoğu oyun motorunun DataModel'inde kullandığı yöntemdir.

### C.2 Instance temel sınıfı

```cpp
// Engine/Core/DataModel/Instance.h

#pragma once
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

class Instance : public std::enable_shared_from_this<Instance> {
public:
    virtual ~Instance() = default;

    std::string name = "Instance";

    // --- Hiyerarşi ---

    void setParent(const std::shared_ptr<Instance>& newParent) {
        if (auto oldParent = parent.lock()) {
            auto& siblings = oldParent->children;
            siblings.erase(
                std::remove(siblings.begin(), siblings.end(), shared_from_this()),
                siblings.end()
            );
        }

        parent = newParent; // weak_ptr ataması — circular reference oluşmaz
        if (newParent) {
            newParent->children.push_back(shared_from_this()); // shared_ptr — güçlü referans
        }
    }

    std::shared_ptr<Instance> findFirstChild(const std::string& childName) const {
        for (auto& child : children)
            if (child->name == childName) return child;
        return nullptr;
    }

    const std::vector<std::shared_ptr<Instance>>& getChildren() const { return children; }
    std::shared_ptr<Instance> getParent() const { return parent.lock(); }

protected:
    std::weak_ptr<Instance> parent;                    // ★ Zayıf referans — circular reference önler
    std::vector<std::shared_ptr<Instance>> children;    // ★ Güçlü referans — ownership burada
};
```

**Neden `std::enable_shared_from_this`?** Çünkü bir `Instance`'ın kendi `shared_ptr`'ını başka bir yerden (setParent içinde) elde edebilmesi gerekiyor. Bu, C++'ın standart kütüphanesinde bu tarz "kendine referans veren" nesneler için sunduğu resmi çözüm.

### C.3 DataModel kök yapısı

```cpp
// Engine/Core/DataModel/DataModel.h

#pragma once
#include "Instance.h"

// Roblox'taki "game" değişkenine karşılık gelir — tüm sahnenin kökü
class DataModel : public Instance {
public:
    DataModel() { name = "DataModel"; }

    std::shared_ptr<Instance> workspace;   // 3D sahne objelerinin yaşadığı yer
    std::shared_ptr<Instance> playerService; // Oyuncu yönetimi (ileriki fazlarda dolacak)

    static DataModel& instance() {
        static DataModel dm;
        return dm;
    }
};
```

### C.4 Yeni obje oluşturma akışı (reflection ile birleşimi)

Buraya kadarki iki sistemi (reflection + DataModel) birleştiren örnek — Editor'de "Insert Part" tuşuna basıldığında olan şey:

```cpp
// Reflection sistemi üzerinden generic obje oluşturma
std::shared_ptr<Instance> createInstance(const std::string& className) {
    auto* classDesc = Engine::Reflection::TypeRegistry::instance().find(className);
    if (!classDesc) return nullptr;

    void* raw = classDesc->factory();                       // "new Part()" tetiklenir
    return std::shared_ptr<Instance>(static_cast<Instance*>(raw));
}

// Kullanım:
auto part = createInstance("Part");
part->name = "MyFirstPart";
part->setParent(DataModel::instance().workspace);
```

Bu kodun önemi: `createInstance("Part")` — burada `"Part"` bir **string**. Yani editör, hangi sınıfların var olduğunu (Part, SpotLight, Sound, vb.) derleme zamanında bilmek zorunda değil, hepsini TypeRegistry'den okuyabiliyor. Yeni bir sınıf eklediğinde (örneğin `SpawnLocation`), editörde hiçbir ek kod yazmadan o sınıf otomatik olarak "Insert Object" menüsünde görünür.

---

## Bölüm D — Serileştirme (Kısa Önizleme)

Faz 1'in son parçası, sahneyi diske kaydedip yükleyebilmek. Reflection sistemi sayesinde bu neredeyse otomatik hale geliyor:

```cpp
// Basitleştirilmiş serileştirme mantığı (JSON örneği)
void serializeInstance(const std::shared_ptr<Instance>& inst, JsonWriter& writer) {
    auto* classDesc = TypeRegistry::instance().find(getClassName(inst));

    writer.beginObject();
    writer.write("ClassName", classDesc->className);
    writer.write("Name", inst->name);

    writer.beginArray("Properties");
    for (auto& prop : classDesc->properties) {
        auto value = prop.getter(inst.get());
        writer.writeProperty(prop.name, value); // std::any'yi type'a göre yazar
    }
    writer.endArray();

    writer.beginArray("Children");
    for (auto& child : inst->getChildren())
        serializeInstance(child, writer); // Recursive — tüm ağaç kaydedilir
    writer.endArray();

    writer.endObject();
}
```

Bu fonksiyonun kritik noktası: **Yeni bir sınıf eklediğinde bu kod değişmiyor.** Çünkü hangi property'lerin kaydedileceğini artık reflection sistemi söylüyor, kod değil.

---

## Bölüm E — Faz 1 "Definition of Done" Kontrol Listesi

Faz 1'in tamamlanmış sayılması için aşağıdakilerin hepsi çalışıyor olmalı:

- [ ] `TypeRegistry` ve `ClassBuilder` derleniyor ve test edilebiliyor
- [ ] En az 3 farklı sınıf (`Instance`, `Part`, `DataModel`) reflection ile kayıt edilmiş
- [ ] `createInstance("Part")` ile string üzerinden obje oluşturulabiliyor
- [ ] `setParent`/`findFirstChild`/`getChildren` ile hiyerarşi kurulabiliyor
- [ ] Bir sahne (birkaç Part içeren basit bir ağaç) JSON'a kaydedilip tekrar yüklenebiliyor
- [ ] Circular reference testi yazılmış — bir Part'ı kendi çocuğu yapmaya çalışınca sistem çökmüyor / anlamlı hata veriyor
- [ ] Bellek sızıntısı testi yapılmış (Valgrind veya benzeri ile) — objeler silindiğinde referans sayısı doğru düşüyor

Bu liste tamamlanmadan Faz 2'ye (Render Pipeline) geçilmemesi öneriliyor — çünkü Faz 2'nin ilk işi, DataModel'deki objeleri okuyup render etmek, ve altyapı sağlam değilse üzerine inşa edilen her şey kırılgan olur.

---

## Sonraki Adım Önerisi

Buradan sonra iki yönden birine gidilebilir:

1. **Faz 2'ye geçiş:** Render pipeline'ın DataModel'i nasıl okuyacağı (scene graph → render queue köprüsü)
2. **Faz 1'in derinleşmesi:** Reflection sistemine kalıtım zincirinin (`Part : Instance`) nasıl tam entegre edileceği, enum/array gibi karmaşık tiplerin property sistemine nasıl ekleneceği

Hangisiyle devam edelim?
