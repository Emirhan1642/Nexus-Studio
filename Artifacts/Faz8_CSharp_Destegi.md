# Faz 8 — Teknik Derinlemesine İnceleme
## C# Desteği: CoreCLR Entegrasyonu ve İkinci Bir Binding Katmanı

Bu doküman, Faz 1'de kurulan reflection sisteminin **ikinci tüketicisi** olarak C#'ı motora bağlamayı inceler. Hedef: Luau bilmeyen ama C#/Unity deneyimi olan bir geliştiricinin, aynı `Part` nesnesine, aynı `workspace` ağacına C# ile de erişebilmesi — Faz 1'de "Aşama 3" olarak planladığımız adım.

**Önemli çerçeveleme:** Bu faz, Faz 3'ün (Luau) bir tekrarı değil — çünkü C#'ın güvenlik modeli, performans profili ve derleme modeli temelden farklı. Bu farklar, bu dokümanın büyük kısmını oluşturuyor.

---

## Bölüm A — Temel Mimari Karar: Hangi .NET Çalışma Zamanı?

### A.1 Seçenekler

| Seçenek | Açıklama | Artı | Eksi |
|---|---|---|---|
| Mono | Unity'nin geleneksel olarak kullandığı runtime | Embedding konusunda olgun, iyi dokümante | Geliştirmesi Microsoft'tan bağımsız, IL2CPP gibi ek karmaşıklıklar |
| **CoreCLR (.NET 8+)** | Modern, Microsoft'un ana .NET runtime'ı | Aktif geliştiriliyor, üstün JIT performansı, resmi "hosting API" | Embedding dokümantasyonu Mono kadar oyun-motoru-odaklı değil |
| IL2CPP (AOT derleme) | C#'ı C++'a çevirip derleme | Script içeren native binary, hızlı | Bizim senaryomuzda (kullanıcı runtime'da script yazıyor) uygulanamaz — AOT, derleme zamanında sabit kod gerektirir |

**Karar: CoreCLR, resmi "Hosting API" (`hostfxr`/`coreclr_delegate`) üzerinden gömülü (embedded) olarak kullanılacak.**

Gerekçe: IL2CPP, kullanıcının runtime'da yeni script yazıp çalıştırdığı bizim senaryomuza uygun değil — o, derleme zamanında belli olan sabit bir kod tabanını native koda çevirmek için tasarlandı. Mono ile CoreCLR arasında ise CoreCLR'ı seçmemizin nedeni, Microsoft'un artık tüm geliştirme kaynağını CoreCLR'a yönlendirmiş olması (Mono, Unity ve Xamarin mirasının dışında giderek daha az öncelikli hale geliyor) — uzun vadeli bir motor projesi için daha sağlam bir temel.

### A.2 CoreCLR'ı gömme (embedding) temelleri

.NET, "hosting" için resmi bir API sunuyor — bu, tarayıcının içine bir JS motoru gömmeye benzer bir işlem:

```cpp
// Engine/Scripting/CSharpRuntime/DotNetHost.h

#pragma once
#include <nethost.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>

class DotNetHost {
public:
    bool initialize(const std::string& runtimeConfigPath) {
        // 1. hostfxr kütüphanesini bul ve yükle (sistemdeki .NET kurulumunu bulur)
        get_hostfxr_path(hostfxrPath, &pathSize, nullptr);
        hostfxrLib = loadLibrary(hostfxrPath);

        // 2. .NET runtime'ı bu process içinde başlat
        hostfxr_initialize_for_runtime_config_fn init = getExport<hostfxr_initialize_for_runtime_config_fn>(
            hostfxrLib, "hostfxr_initialize_for_runtime_config"
        );
        init(runtimeConfigPath.c_str(), nullptr, &hostContext);

        // 3. Yönetilen (managed) kod tarafına fonksiyon işaretçisi almak için delegate al
        hostfxr_get_runtime_delegate_fn getDelegate = getExport<hostfxr_get_runtime_delegate_fn>(
            hostfxrLib, "hostfxr_get_runtime_delegate"
        );
        getDelegate(hostContext, hdt_load_assembly_and_get_function_pointer, (void**)&loadAssemblyFn);

        return true;
    }

    // Bir C# assembly'sindeki [UnmanagedCallersOnly] işaretli statik fonksiyonu C++'a bağlar
    template<typename FuncPtr>
    FuncPtr getManagedFunction(const std::string& assemblyPath, const std::string& typeName,
                                 const std::string& methodName) {
        FuncPtr fn = nullptr;
        loadAssemblyFn(assemblyPath.c_str(), typeName.c_str(), methodName.c_str(),
                        UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&fn);
        return fn;
    }

private:
    hostfxr_handle hostContext;
    load_assembly_and_get_function_pointer_fn loadAssemblyFn;
    // ... hostfxrLib, pathSize vb.
};
```

**Bu, Faz 3'teki Luau başlatmasından temelde farklı:** Luau, C++ içine doğrudan gömülen küçük bir kütüphaneyken (birkaç MB), .NET runtime'ı ayrı bir sistem bileşeni — kullanıcı bilgisayarında .NET runtime kurulu olmalı (veya motor kendi yanında "self-contained" bir kopyasını taşımalı, bu da dağıtım boyutunu ciddi büyütür). Bu, Faz 8'in "hafiflik" hedefiyle gerilim yaratan ilk nokta — Bölüm F'de bu gerilim ele alınıyor.

---

## Bölüm B — Reflection Köprüsü: C++ ⟷ C#

### B.1 Luau'dan temel fark: P/Invoke ve `[UnmanagedCallersOnly]`

Luau binding'inde (Faz 3) C++ tarafı Lua'nın C API'sini (`lua_State*`, `lua_pushnumber` vb.) doğrudan çağırıyordu. C#'ta bu ilişki tam tersine çevrilir: **C#, C++ fonksiyonlarını P/Invoke ile çağırır; C++ de C# fonksiyonlarını `[UnmanagedCallersOnly]` işaretli statik metodlar üzerinden çağırır.** İki yönlü bir köprü.

### B.2 C++ tarafı — reflection verisini C#'a expose etme

Faz 1'deki `TypeRegistry`'ye dokunmadan, üzerine ince bir P/Invoke katmanı ekliyoruz:

```cpp
// Engine/Scripting/CSharpRuntime/ReflectionInterop.cpp

extern "C" {
    // C# tarafından çağrılacak, property okuma
    __declspec(dllexport) void Engine_GetProperty(InstanceId id, const char* propName, PropertyValueOut* out) {
        auto inst = InstanceRegistry::instance().findById(id);
        if (!inst) { out->kind = PropertyValueOut::Kind::Null; return; }

        auto* classDesc = TypeRegistry::instance().find(getClassName(inst));
        auto* prop = classDesc->findProperty(propName);
        if (!prop) { out->kind = PropertyValueOut::Kind::Error; return; }

        std::any value = prop->getter(inst.get());
        marshalAnyToInterop(value, prop->kind, out); // std::any -> C ABI uyumlu struct
    }

    // C# tarafından çağrılacak, property yazma
    __declspec(dllexport) void Engine_SetProperty(InstanceId id, const char* propName, const PropertyValueIn* value) {
        auto inst = InstanceRegistry::instance().findById(id);
        auto* classDesc = TypeRegistry::instance().find(getClassName(inst));
        auto* prop = classDesc->findProperty(propName);
        if (!prop || prop->readOnly) return;

        std::any anyValue = marshalInteropToAny(value, prop->kind);
        prop->setter(inst.get(), anyValue); // ★ Faz 1'deki AYNI setter — Luau ile birebir aynı zincir tetiklenir
    }
}
```

**Kritik nokta:** `Engine_SetProperty`, Faz 1'deki `PropertyDescriptor::setter`'ı çağırıyor — bu da (Faz 2, 5, 6'da gördüğümüz gibi) RenderProxy, Jolt body, ve replication sistemini otomatik tetikliyor. C# binding'i, Luau binding'inin **paralel bir kopyası değil, aynı merkezi noktaya bağlanan ikinci bir müşteri.**

### B.3 C# tarafı — reflection'ı doğal C# sözdizimine saran wrapper

C# tarafında amaç, geliştiricinin `Engine_GetProperty("id", "Position")` gibi çirkin bir API yerine, doğal `part.Position` sözdizimi kullanabilmesi:

```csharp
// EngineSharp/Instance.cs

public class Instance
{
    internal uint InstanceId { get; }

    protected T GetProperty<T>(string name)
    {
        NativeInterop.PropertyValueOut result = default;
        NativeMethods.Engine_GetProperty(InstanceId, name, ref result);
        return InteropMarshal.Convert<T>(result);
    }

    protected void SetProperty<T>(string name, T value)
    {
        var interopValue = InteropMarshal.ConvertToInterop(value);
        NativeMethods.Engine_SetProperty(InstanceId, name, ref interopValue);
    }
}

public class Part : Instance
{
    public Vector3 Position
    {
        get => GetProperty<Vector3>("Position");
        set => SetProperty("Position", value);
    }

    public bool Anchored
    {
        get => GetProperty<bool>("Anchored");
        set => SetProperty("Anchored", value);
    }
}
```

### B.4 Kritik tasarım sorusu: `Part` sınıfı C# tarafında elle mi yazılacak, otomatik mi üretilecek?

Bölüm B.3'teki `Part` sınıfı elle yazılmış gibi görünüyor — ama bu, yeni bir C++ sınıfı eklendiğinde C# tarafında da elle güncelleme yapmayı gerektirir, bu da Faz 1'in "tek kayıt noktası" felsefesine aykırı düşer.

**Çözüm: Build-time code generation.** Derleme sürecine, `TypeRegistry`'deki tüm sınıfları gezip yukarıdaki gibi C# dosyaları üreten bir adım ekleniyor:

```cpp
// Tools/CSharpBindingGenerator/main.cpp — derleme sırasında bir kez çalışan yardımcı araç

void generateCSharpBindings(const std::string& outputDir) {
    for (auto& [className, classDesc] : TypeRegistry::instance().getAllClasses()) {
        std::ofstream file(outputDir + "/" + className + ".g.cs"); // ".g.cs" = generated, elle düzenlenmez

        file << "public class " << className << " : " << (classDesc.baseClass ? classDesc.baseClass->className : "object") << " {\n";
        for (auto& prop : classDesc.properties) {
            std::string csharpType = mapToCSharpType(prop.kind, prop.typeName);
            file << "    public " << csharpType << " " << prop.name << " {\n";
            file << "        get => GetProperty<" << csharpType << ">(\"" << prop.name << "\");\n";
            if (!prop.readOnly)
                file << "        set => SetProperty(\"" << prop.name << "\", value);\n";
            file << "    }\n";
        }
        file << "}\n";
    }
}
```

**Bu, Unreal'ın UnrealHeaderTool'una kavramsal olarak benziyor** ama çok daha basit bir versiyonu — çünkü bizim reflection sistemimiz zaten runtime'da var, sadece onu C#'a "yansıtan" statik dosyalar üretiyoruz. Bu araç, CMake build sürecinin bir adımı olarak her derlemede otomatik çalışır (Faz 0'daki modüler CMake yapısına yeni bir custom target olarak eklenir).

---

## Bölüm C — Güvenlik Modeli: Neden C# Luau Gibi Sandbox'lanamaz

### C.1 Temel fark

Faz 3, Bölüm C'de Luau'yu sandbox'lamıştık — `io`, `os.execute`, `package` gibi tehlikeli kütüphaneleri kaldırarak. C#'ta bu **aynı şekilde çalışmıyor**, çünkü C# derlenmiş IL kodu doğrudan CLR üzerinde çalışıyor ve `System.IO.File`, `System.Diagnostics.Process` gibi güçlü sistem API'lerine erişim, dilin kendisinin bir parçası — Luau'daki gibi "birkaç global fonksiyonu kaldır" ile çözülemez.

### C.2 Bunun pratik sonucu: Güven modeli ayrışması

**Karar: C# scriptleri, yalnızca "güvenilir" (trusted) bağlamlarda çalışabilir.**

```cpp
enum class ScriptTrustLevel {
    Untrusted,  // Kullanıcı tarafından yazılan, güvenilmeyen kod — SADECE Luau
    Trusted,    // Motor geliştiricisi veya proje sahibi tarafından yazılan kod — Luau VEYA C#
};
```

Somut anlamı:
- Roblox tarzı bir platformda (birden fazla kullanıcının aynı sunucuya script yükleyebildiği bir senaryo), **C# kullanımı kapalı tutulmalı** — çünkü bir C# scripti `System.IO.File.Delete("C:\\")` yazabilir ve hiçbir sandbox bunu önleyemez.
- Ama tek bir stüdyonun kendi kapalı projesinde (geliştiricilerin zaten güvenilir olduğu bir ortamda — tıpkı Unity/Unreal projelerinde olduğu gibi), C# kullanımı tamamen makul, çünkü zaten o motoru derleyen kişiler projenin sahipleri.

**Bu ayrım, editörde açıkça görünür olmalı:** Bir proje "Community/Multiplayer Platform" modunda mı yoksa "Standalone Studio" modunda mı çalıştığını baştan seçmeli, ve C# desteği sadece ikincisinde aktif olmalı. Bu, Roblox'un neden hâlâ (yıllar sonra bile) sadece Luau'ya izin verdiğinin gerçek nedenidir — bu bir teknik eksiklik değil, bilinçli bir güvenlik kararıdır.

---

## Bölüm D — Performans Karşılaştırması ve Kullanım Rehberi

### D.1 Luau vs C# — ne zaman hangisi

| Kriter | Luau | C# |
|---|---|---|
| Başlangıç (cold start) süresi | Çok hızlı (VM zaten gömülü, ms mertebesinde) | Yavaş (CLR başlatma yüzlerce ms sürebilir) |
| Sıcak döngü (hot loop) performansı | Yorumlanan bytecode, orta | JIT derlenmiş, C++'a yakın |
| Hot-reload (kod değiştir, sahneyi yeniden başlatmadan gör) | Trivial (script yeniden compile edilip resume edilir) | Karmaşık (.NET'in "hot reload" özelliği var ama sınırlı, bazı değişiklikler için CLR'ın yeniden başlatılması gerekebilir) |
| Bellek ayak izi (binlerce script örneği) | Düşük | Yüksek (her AppDomain/AssemblyLoadContext ekstra yük getirir) |
| Güvenlik | Sandbox'lanabilir | Sandbox'lanamaz (Bölüm C) |

**Pratik öneri (editörde geliştiriciye gösterilecek rehber niteliğinde):** Luau, sık değişen, hot-reload gerektiren, "hafiflik" öncelikli gameplay mantığı için; C#, performans-kritik, matematik-ağır sistemler (örn. büyük ölçekli prosedürel üretim, karmaşık AI, veri işleme) için tercih edilmeli. Bu, tam olarak Unreal'ın "Blueprint hızlı iterasyon için, C++ performans için" felsefesinin bizim motorumuzdaki karşılığı.

### D.2 İkisinin aynı sahnede birlikte çalışması

Reflection köprüsü sayesinde, bir Part'ın `Touched` event'ini bir Luau script'i dinlerken, başka bir Part'ın `Heartbeat`'ini bir C# scripti işleyebilir — ikisi de aynı `Instance` ağacına, aynı reflection API'sine bakıyor, birbirlerinden habersiz ama tutarlı şekilde çalışabiliyorlar. Bu, Godot'un GDScript+C# birlikte çalışma modeline benziyor (Faz 3 öncesi ilk konuşmamızda bahsettiğimiz "Yol 2" modeli).

---

## Bölüm E — Script Timeout Konusunun C# Tarafındaki Karşılığı

Önceki dokümanda (Script Timeout) Luau için `interrupt` callback mekanizmasını detaylandırmıştık. C#'ta bu **doğrudan bir karşılığı yok** — CLR, Luau gibi "her N instruction'da bir beni durdur" diyen bir hook sunmuyor. Bu, C#'ın Untrusted bağlamlarda kullanılamamasının (Bölüm C.2) bir başka gerekçesi:

```cpp
// C# tarafında sonsuz döngü koruması için mevcut, ama kısıtlı seçenekler:

// Seçenek 1: CancellationToken deseni — ama bu, C# KODUNUN KENDİSİNİN
// düzenli aralıklarla token.ThrowIfCancellationRequested() çağırmasını gerektirir.
// Yani Luau'daki gibi "geliştiricinin kontrolü dışında" bir koruma DEĞİL.

// Seçenek 2: Ayrı bir AppDomain/AssemblyLoadContext'te çalıştırıp, zaman aşımında
// o context'i tamamen boşaltmak (unload). Bu çalışır ama script'in TÜM state'ini kaybeder
// (Luau'daki gibi "hatayı yakala, script'i durdur, diğerleri devam etsin" kadar zarif değil).
```

**Sonuç:** C# desteği, Bölüm C.2'deki güven modeliyle birlikte düşünüldüğünde tutarlı hale geliyor — zaten sadece "Trusted" (proje sahibinin yazdığı) kod C# çalıştırabildiği için, bu kodun kasıtlı olarak sonsuz döngüye sokulması bir güvenlik tehdidi değil, normal bir geliştirme hatası olarak ele alınabilir (Visual Studio/Rider'da debugger ile durdurulur, tıpkı normal bir C# uygulamasında olduğu gibi).

---

## Bölüm F — Dağıtım (Distribution) Sorunu ve Hafiflik Hedefiyle Gerilim

### F.1 Sorun

Faz 0'ın temel hedeflerinden biri "Roblox Studio kadar hafif" olmaktı. .NET runtime'ının kendisi (self-contained dağıtımda) yüzlerce MB'a varabilir. Bu, C# desteğinin **varsayılan olarak açık olmaması gerektiği** anlamına geliyor.

### F.2 Çözüm: Opsiyonel, talep üzerine indirilen bir modül

```cpp
// Editor/ProjectSettings.h

struct ProjectSettings {
    bool csharpSupportEnabled = false; // ★ Varsayılan KAPALI

    void enableCSharpSupport() {
        if (!isDotNetRuntimeInstalled()) {
            EditorApp::instance().promptDownload(
                "C# desteği için .NET Runtime indirilmesi gerekiyor (~60MB). İndirilsin mi?"
            );
        }
        csharpSupportEnabled = true;
    }
};
```

Bu, `DotNetHost::initialize()` (Bölüm A.2) çağrısının **motor her açıldığında değil, sadece kullanıcı C# desteğini bir proje için açıkça etkinleştirdiğinde** tetiklenmesi anlamına geliyor. Bu sayede saf Luau kullanan bir kullanıcı hiçbir zaman .NET runtime yükü ödemiyor — "hafiflik" hedefi C# desteğiyle bir kerede feda edilmemiş oluyor.

---

## Bölüm G — Faz 8 "Definition of Done" Kontrol Listesi

- [ ] CoreCLR, opsiyonel bir proje ayarıyla başlatılabiliyor; kapalıyken hiçbir performans/bellek yükü getirmiyor
- [ ] `Tools/CSharpBindingGenerator`, derleme sırasında TypeRegistry'deki tüm sınıflar için otomatik C# dosyaları üretiyor
- [ ] Bir C# scriptinden `part.Position = new Vector3(0,10,0)` çalıştığında, Faz 2/5/6'daki tüm zincir (render, fizik, replication) Luau'dakiyle birebir aynı şekilde tetikleniyor
- [ ] Aynı sahnede bir Luau scripti ve bir C# scripti aynı Part'a farklı yönlerden erişip birbirini bozmadan çalışabiliyor (eşzamanlılık testi)
- [ ] `ScriptTrustLevel::Untrusted` bağlamında (örn. multiplayer platform modu) C# seçeneği editörde tamamen gizli/devre dışı
- [ ] Yeni bir C++ sınıfı (`ClassBuilder<NewClass>`) eklendiğinde, bir sonraki derlemede C# tarafında otomatik olarak `NewClass.g.cs` üretildiği doğrulanmış
- [ ] .NET runtime kurulu olmayan bir makinede, C# desteği ilk etkinleştirildiğinde kullanıcıya indirme istemi doğru şekilde çıkıyor

---

## Sonraki Adım Önerisi

Faz 8 ile birlikte ana teknoloji yığınının (fizik, render, scripting x2, networking, editör) tamamı bir kez uçtan uca ele alınmış oldu. Önceki fazlarda bilinçli olarak ertelenen konular hâlâ bekliyor:

1. **Karakter kontrolcüsü (Humanoid sistemi):** Faz 5'ten beri bekleyen, fizik + networking + artık her iki scripting dilini de kapsayan gameplay sistemi.
2. **Asset Browser / import akışı:** Faz 4'ten beri bekleyen editör tarafı eksik.
3. **Interest Management'in derinleştirilmesi:** Faz 6'da basit tutulan relevancy sisteminin gerçek bir Replication Graph'a evrilmesi.

Hangisiyle devam edelim?
