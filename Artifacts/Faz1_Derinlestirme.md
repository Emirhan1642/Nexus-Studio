# Faz 1 — Derinleştirme
## Kalıtım Zinciri Entegrasyonu ve Karmaşık Property Tipleri

Bu doküman, "Faz0_Faz1_Teknik_Detay.md" dosyasındaki temel reflection sistemini genişletir. Önceki dokümanda `TypeRegistry`, `ClassBuilder` ve basit `property()` çağrısı tasarlanmıştı. Burada üç açık soruyu çözüyoruz:

1. `base("Instance")` çağrısı güvenilir mi? (Static initialization order sorunu)
2. `Part.IsA("Instance")` gibi bir sorgu nasıl çalışır?
3. Enum, array, ve başka bir Instance'a referans tutan property'ler (Object reference) nasıl desteklenir?

---

## Bölüm A — Kalıtım Zincirinin Güvenilir Hale Getirilmesi

### A.1 Gizli tehlike: Static Initialization Order Fiasco

Önceki dokümandaki şu koda dikkatlice bak:

```cpp
ClassBuilder<Part>("Part")
    .base("Instance")   // ← "Instance" sınıfı bu ana kadar KAYIT EDİLMİŞ Mİ?
    .property("Position", &Part::position)
```

Bu satır çalıştığında `TypeRegistry::instance().find("Instance")` çağrılıyor. Sorun şu: **C++ standardı, farklı `.cpp` dosyalarındaki global/statik nesnelerin hangi sırayla initialize edileceğini garanti etmez.** Yani `Instance.cpp` dosyasındaki registration kodu, `Part.cpp` dosyasındaki registration kodundan **önce mi sonra mı** çalışacak — bu derleyiciye, hatta build'den build'e değişebilir.

Bu, C++'ta bilinen ve "Static Initialization Order Fiasco" adıyla anılan klasik bir tuzaktır. Eğer `Part` önce register edilirse, `base("Instance")` çağrısı `nullptr` bulur ve kalıtım zinciri sessizce kırılır — hiçbir hata mesajı almadan `Part`'ın `Instance`'tan miras aldığı property'leri (örn. `Name`) editörde göremezsin.

### A.2 Çözüm: İki Aşamalı (Deferred) Registration

Sorunu kökünden çözmek için registration'ı **iki aşamaya** bölüyoruz:

- **Aşama 1 (program başlarken, sıra garantisiz):** Her sınıf sadece "ben varım, adım şu, base class'ımın adı şu" bilgisini bir bekleme listesine yazar. Henüz gerçek bağlantı kurulmaz.
- **Aşama 2 (main() başında, tek seferlik):** `Reflection::finalize()` çağrılır. Bu noktada tüm sınıflar zaten kayıtlı olduğu için, kalıtım zincirleri güvenle kurulur.

```cpp
// Engine/Core/Reflection/TypeRegistry.h (genişletilmiş)

namespace Engine::Reflection {

struct PendingClassInfo {
    std::string className;
    std::string baseClassName;   // Henüz çözülmemiş — sadece isim
};

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

    // Aşama 1'de çağrılır — sadece isim kaydeder, henüz bağlamaz
    void deferBaseClass(const std::string& className, const std::string& baseName) {
        pending.push_back({className, baseName});
    }

    // Aşama 2 — main() başında BİR KEZ çağrılır
    void finalize() {
        for (auto& p : pending) {
            ClassDescriptor* derived = find(p.className);
            ClassDescriptor* base = find(p.baseClassName);

            if (!base) {
                // Artık sessizce geçilmiyor — geliştirici anında haberdar oluyor
                throw std::runtime_error(
                    "Reflection hatası: '" + p.className +
                    "' sınıfının base'i '" + p.baseClassName + "' bulunamadı. "
                    "Yazım hatası mı var, yoksa base class hiç register edilmemiş mi?"
                );
            }
            derived->baseClass = base;
        }
        pending.clear();
    }

    ClassDescriptor* find(const std::string& name) {
        auto it = classes.find(name);
        return it != classes.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, ClassDescriptor> classes;
    std::vector<PendingClassInfo> pending;
};

} // namespace Engine::Reflection
```

`ClassBuilder::base()` artık şöyle görünüyor — artık anında arama yapmıyor, sadece isteği kaydediyor:

```cpp
ClassBuilder& base(const std::string& baseName) {
    TypeRegistry::instance().deferBaseClass(descriptor->className, baseName);
    return *this;
}
```

Ve `main.cpp`'de (ya da `Editor/Main.cpp`'nin en başında), tüm statik registration'lar tamamlandıktan sonra:

```cpp
int main() {
    Engine::Reflection::TypeRegistry::instance().finalize(); // ★ Kritik satır
    // ... geri kalan başlatma kodu
}
```

**Bu tasarımın kazandırdığı şey:** Artık hangi `.cpp` dosyasının önce derlendiği önemli değil. `finalize()` çağrıldığında tüm sınıflar zaten evrende var, kalıtım zincirleri güvenle kurulabiliyor. Ayrıca eksik/yanlış yazılmış bir base class ismi artık **sessizce yutulmuyor, program başlarken anında patlıyor** — bu tür hataları prod'da değil, geliştirme sırasında yakalamak çok değerli.

### A.3 `IsA()` sorgusu — Roblox'taki `part:IsA("BasePart")` deneyimi

Kalıtım zinciri artık güvenilir olduğuna göre, üstüne bir tip sorgulama API'si ekleyebiliriz:

```cpp
// ClassDescriptor içine eklenir
bool isA(const std::string& targetClassName) const {
    const ClassDescriptor* current = this;
    while (current) {
        if (current->className == targetClassName) return true;
        current = current->baseClass;
    }
    return false;
}
```

Kullanım (ileride Luau binding'inde `part:IsA("BasePart")` olarak görünecek şey aslında budur):

```cpp
auto* desc = TypeRegistry::instance().find("Part");
desc->isA("Instance"); // true — çünkü Part -> Instance zinciri kuruldu
desc->isA("Part");     // true — kendisi
desc->isA("SpotLight"); // false
```

---

## Bölüm B — Karmaşık Property Tipleri

### B.1 Mevcut sistemin sınırı

Önceki dokümandaki `property()` fonksiyonu `std::any` kullanıyordu. Bu, `float`, `bool`, `Vector3` gibi "kendi kendine yeten" tipler için sorunsuz çalışır. Ama şu senaryolarda yetersiz kalır:

- **Enum:** `Part.Material = Material::Wood` — editörde bunun bir dropdown olarak görünmesi, olası değerlerin (`Wood`, `Metal`, `Plastic`) listelenebilmesi gerekir. `std::any` bunu bilmiyor.
- **Array/Liste:** `Model.Tags = {"Enemy", "Breakable"}` gibi bir string listesi.
- **Object Reference:** Bir property'nin başka bir `Instance`'a işaret etmesi (örn. bir `Weld`'in `Part0`/`Part1` alanları).

Her biri için ayrı bir çözüm tasarlıyoruz.

### B.2 Enum desteği

Önce bir Enum registry kuruyoruz — bu, C++ enum'unu isim ⟷ değer olarak runtime'da tanıtır:

```cpp
// Engine/Core/Reflection/EnumRegistry.h

namespace Engine::Reflection {

struct EnumDescriptor {
    std::string enumName;
    std::vector<std::pair<std::string, int>> values; // {"Wood", 0}, {"Metal", 1} ...

    std::string nameOf(int value) const {
        for (auto& [name, v] : values) if (v == value) return name;
        return "Unknown";
    }
    int valueOf(const std::string& name) const {
        for (auto& [n, v] : values) if (n == name) return v;
        return -1;
    }
};

class EnumRegistry {
public:
    static EnumRegistry& instance() {
        static EnumRegistry reg;
        return reg;
    }

    EnumDescriptor& registerEnum(const std::string& name) {
        auto& desc = enums[name];
        desc.enumName = name;
        return desc;
    }

    EnumDescriptor* find(const std::string& name) {
        auto it = enums.find(name);
        return it != enums.end() ? &it->second : nullptr;
    }

private:
    std::unordered_map<std::string, EnumDescriptor> enums;
};

} // namespace Engine::Reflection
```

Kayıt örneği:

```cpp
enum class Material { Wood, Metal, Plastic, Concrete };

// Program başlarken (Part.cpp içinde, Part registration'ından önce)
namespace {
    struct MaterialEnumInit {
        MaterialEnumInit() {
            using namespace Engine::Reflection;
            auto& e = EnumRegistry::instance().registerEnum("Material");
            e.values = {
                {"Wood", (int)Material::Wood},
                {"Metal", (int)Material::Metal},
                {"Plastic", (int)Material::Plastic},
                {"Concrete", (int)Material::Concrete}
            };
        }
    } g_materialEnumInit;
}
```

`PropertyDescriptor`'a enum bilgisini taşıyacak bir alan ekliyoruz:

```cpp
struct PropertyDescriptor {
    std::string name;
    std::function<std::any(void*)> getter;
    std::function<void(void*, const std::any&)> setter;

    enum class Kind { Primitive, Enum, Array, ObjectRef } kind = Kind::Primitive;
    std::string typeName;       // "float", "Vector3" vb.
    std::string enumTypeName;   // Kind::Enum ise → "Material"
};
```

Ve `ClassBuilder`'a enum'a özel bir overload ekliyoruz:

```cpp
template<typename EnumT>
ClassBuilder& enumProperty(const std::string& name, EnumT T::* member, const std::string& enumTypeName) {
    PropertyDescriptor desc;
    desc.name = name;
    desc.kind = PropertyDescriptor::Kind::Enum;
    desc.enumTypeName = enumTypeName;

    desc.getter = [member](void* instance) -> std::any {
        return static_cast<int>(static_cast<T*>(instance)->*member);
    };
    desc.setter = [member](void* instance, const std::any& value) {
        static_cast<T*>(instance)->*member = static_cast<EnumT>(std::any_cast<int>(value));
    };

    descriptor->properties.push_back(std::move(desc));
    return *this;
}
```

Kullanım:

```cpp
ClassBuilder<Part>("Part")
    .base("Instance")
    .property("Position", &Part::position)
    .enumProperty("Material", &Part::material, "Material"); // ★ yeni
```

**Editördeki karşılığı:** Properties paneli, bir property'nin `kind == Kind::Enum` olduğunu gördüğünde, `EnumRegistry::instance().find("Material")` çağırıp dönen `values` listesinden otomatik bir dropdown çizer. Yeni bir enum değeri eklendiğinde (`Rubber` gibi) editör kodunda hiçbir değişiklik gerekmez.

### B.3 Array/Liste desteği

Array'ler için `std::any` içine doğrudan `std::vector<T>` koymak çalışır, ama editörün "bu bir listedir, elemanları tek tek düzenlenebilir" bilgisine ihtiyacı var. Bunun için `Kind::Array` ve elemana erişim için ayrı bir arayüz tanımlıyoruz:

```cpp
struct PropertyDescriptor {
    // ... önceki alanlar ...

    // Sadece Kind::Array olan property'lerde dolu olur
    std::function<size_t(void* instance)> arraySize;
    std::function<std::any(void* instance, size_t index)> arrayGet;
    std::function<void(void* instance, size_t index, const std::any&)> arraySet;
};

template<typename ElemT>
ClassBuilder& arrayProperty(const std::string& name, std::vector<ElemT> T::* member) {
    PropertyDescriptor desc;
    desc.name = name;
    desc.kind = PropertyDescriptor::Kind::Array;

    desc.arraySize = [member](void* instance) -> size_t {
        return (static_cast<T*>(instance)->*member).size();
    };
    desc.arrayGet = [member](void* instance, size_t i) -> std::any {
        return (static_cast<T*>(instance)->*member)[i];
    };
    desc.arraySet = [member](void* instance, size_t i, const std::any& value) {
        (static_cast<T*>(instance)->*member)[i] = std::any_cast<ElemT>(value);
    };

    descriptor->properties.push_back(std::move(desc));
    return *this;
}
```

Kullanım:

```cpp
class Model : public Instance {
public:
    std::vector<std::string> tags;
};

ClassBuilder<Model>("Model")
    .base("Instance")
    .arrayProperty("Tags", &Model::tags);
```

### B.4 Object Reference — bir Instance'ın başka bir Instance'a işaret etmesi

Bu, en hassas kısım. Örneğin bir `WeldConstraint`'in `Part0` ve `Part1` alanları başka `Part` nesnelerine referans tutar. Burada bellek yönetimi kararımızla (Bölüm C, önceki doküman) çelişmemek çok önemli — bu referanslar **weak_ptr** olmalı, çünkü bir Weld, işaret ettiği Part'ın "sahibi" değildir; Part silinirse Weld'in referansı da geçersiz hale gelmeli, tersi olmamalı.

```cpp
class WeldConstraint : public Instance {
public:
    std::weak_ptr<Instance> part0;
    std::weak_ptr<Instance> part1;
};

struct PropertyDescriptor {
    // ... önceki alanlar ...
    // Kind::ObjectRef için:
    std::function<std::shared_ptr<Instance>(void*)> objectGetter;
    std::function<void(void*, std::shared_ptr<Instance>)> objectSetter;
};

ClassBuilder& objectProperty(
    const std::string& name,
    std::weak_ptr<Instance> T::* member
) {
    PropertyDescriptor desc;
    desc.name = name;
    desc.kind = PropertyDescriptor::Kind::ObjectRef;

    desc.objectGetter = [member](void* instance) -> std::shared_ptr<Instance> {
        return (static_cast<T*>(instance)->*member).lock(); // weak_ptr → shared_ptr (varsa)
    };
    desc.objectSetter = [member](void* instance, std::shared_ptr<Instance> value) {
        (static_cast<T*>(instance)->*member) = value; // shared_ptr → weak_ptr (otomatik)
    };

    descriptor->properties.push_back(std::move(desc));
    return *this;
}
```

**Neden bu kritik:** Eğer burada yanlışlıkla `shared_ptr` kullanılsaydı, bir `WeldConstraint` sahne kaydında dolaylı olarak bir `Part`'ı hayatta tutmaya devam ederdi — kullanıcı o Part'ı sahneden silse bile bellekte "hayalet" olarak kalırdı. `weak_ptr` kullanmak, "referans veriyorum ama sahiplenmiyorum" anlamına geldiği için DataModel'in ownership modeliyle (Bölüm C.1, önceki doküman) tutarlı kalıyor.

---

## Bölüm C — Editör Metadata'sı (Bonus, Faz 4'ü kolaylaştırır)

Faz 4'e geldiğinde Properties panelinin property'leri kategorilere ayırması (`Data`, `Appearance`, `Behavior` gibi), bazılarının salt-okunur gösterilmesi gerekecek. Bunu şimdiden `PropertyDescriptor`'a eklemek, Faz 4'te editör tarafında hiçbir reflection değişikliği gerektirmeyecek:

```cpp
struct PropertyDescriptor {
    // ... önceki alanlar ...
    std::string category = "Data";  // Properties panelinde gruplama için
    bool readOnly = false;          // true ise editörde gri, düzenlenemez gösterilir
    std::string tooltip;            // Fare üzerine gelince açıklama
};

// ClassBuilder'a küçük bir zincirleme eklentisi:
ClassBuilder& category(const std::string& cat) {
    if (!descriptor->properties.empty())
        descriptor->properties.back().category = cat;
    return *this;
}
```

Kullanım:

```cpp
ClassBuilder<Part>("Part")
    .base("Instance")
    .property("Position", &Part::position).category("Data")
    .property("Transparency", &Part::transparency).category("Appearance")
    .property("Anchored", &Part::anchored).category("Behavior");
```

---

## Bölüm D — Metod (Fonksiyon) Reflection'ı

Faz 3'te Luau'dan `part:Destroy()` gibi bir çağrı yapılabilmesi için property'lerin yanı sıra **metodların** da reflection sistemine kayıt edilmesi gerekiyor. Bunu şimdiden temel taşlarını atmak, Faz 3'ü kolaylaştırır:

```cpp
struct MethodDescriptor {
    std::string name;
    std::function<std::any(void* instance, std::vector<std::any> args)> invoke;
};

// ClassDescriptor'a eklenir:
std::vector<MethodDescriptor> methods;

// ClassBuilder'a eklenir:
template<typename Ret, typename... Args>
ClassBuilder& method(const std::string& name, Ret (T::*fn)(Args...)) {
    MethodDescriptor desc;
    desc.name = name;
    desc.invoke = [fn](void* instance, std::vector<std::any> args) -> std::any {
        T* obj = static_cast<T*>(instance);
        // NOT: Gerçek implementasyonda args paketinden tek tek unpack edilip
        // fn çağrılır (index_sequence tekniğiyle). Burada kavramsal iskelet gösteriliyor.
        return invokeWithUnpackedArgs(obj, fn, args, std::index_sequence_for<Args...>{});
    };
    descriptor->methods.push_back(std::move(desc));
    return *this;
}
```

Kullanım:

```cpp
class Instance {
public:
    void destroy() { setParent(nullptr); /* ... temizlik ... */ }
};

ClassBuilder<Instance>("Instance")
    .method("Destroy", &Instance::destroy);
```

> **Not:** `args` paketini `std::any`'den gerçek parametre tiplerine açan (`invokeWithUnpackedArgs`) fonksiyonun tam implementasyonu template metaprogramming gerektirir ve bu dokümanın kapsamı dışında bırakılmıştır — Faz 3'te Luau binding'i yazılırken birlikte ele alınacak, çünkü Luau'dan gelen argümanların tipi zaten orada çözülüyor olacak.

---

## Bölüm E — Güncellenmiş "Definition of Done" Kontrol Listesi

Önceki dokümandaki listeye ek olarak:

- [ ] `TypeRegistry::finalize()` çağrılıyor ve `main()`'in en başında çalıştığı doğrulanmış
- [ ] Yanlış/eksik bir `base()` ismi verildiğinde program **anlamlı bir hata mesajıyla** çöküyor (sessizce geçmiyor)
- [ ] `ClassDescriptor::isA()` en az 3 seviyeli bir kalıtım zincirinde test edilmiş (örn. `Part -> BasePart -> Instance`)
- [ ] En az bir enum property (`Material` gibi) kayıt edilmiş ve editör olmadan, birim testle getter/setter'ının doğru çalıştığı doğrulanmış
- [ ] En az bir array property (`Tags` gibi) kayıt edilmiş
- [ ] En az bir Object Reference property (`WeldConstraint.Part0` gibi) kayıt edilmiş ve **weak_ptr olduğu, referans edilen Part silindiğinde `lock()`'un `nullptr` döndüğü** test edilmiş
- [ ] En az bir metod (`Instance::Destroy`) kayıt edilmiş ve generic `invoke()` üzerinden çağrılabildiği doğrulanmış

---

## Sonraki Adım Önerisi

Faz 1 artık hem kalıtım hem karmaşık tipler açısından sağlam bir temele oturdu. Buradan üç yöne gidilebilir:

1. **Faz 2'ye geçiş:** Render pipeline'ın bu DataModel'i nasıl okuyup ekrana çizeceği (scene graph → render queue köprüsü, `Part` gibi sınıfların nasıl "render edilebilir" olarak işaretleneceği)
2. **Faz 3'ün öne çekilmesi:** Luau binding'inin bu reflection sistemi üzerinden nasıl kurulacağı — `part.Position = Vector3.new(0,10,0)` satırının C++ tarafında tam olarak hangi adımlardan geçtiği
3. **Serileştirme sisteminin tam detaylandırılması:** Enum/Array/ObjectRef tiplerinin JSON'a nasıl yazılıp geri okunacağı, sahne dosyası formatının tam şeması

Hangisiyle devam edelim?
