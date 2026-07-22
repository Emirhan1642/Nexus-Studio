# Faz 3 — Teknik Derinlemesine İnceleme
## Luau Scripting Entegrasyonu: Reflection'dan Script'e Köprü

Bu doküman, Faz 1'de kurulan reflection sisteminin (TypeRegistry, ClassBuilder, PropertyDescriptor) Luau VM'ine nasıl bağlanacağını inceler. Hedef: `part.Position = Vector3.new(0, 10, 0)` gibi bir satırın, C++ tarafında hangi adımlardan geçtiğini netleştirmek.

---

## Bölüm A — Luau VM'i Gömme (Embedding) Temelleri

### A.1 Luau'nun standart Lua'dan farkı

Luau, Roblox'un standart Lua 5.1 üzerine inşa ettiği bir türev. Bizim için önemli üç fark:

- **Yerleşik sandboxing desteği:** Luau, `getfenv`/`setfenv` gibi güvenlik açığı yaratabilecek tehlikeli fonksiyonları baştan kaldırmış. Standart Lua'da bunları elle kapatman gerekirdi.
- **Bytecode derleyicisi ayrık:** Kaynak kod (`.lua`), `Luau::compile()` ile önce bytecode'a çevrilir, sonra VM bu bytecode'u çalıştırır. Bu ayrım, ileride "scripti sunucuda derle, bytecode'u istemciye gönder" gibi bir optimizasyona izin verir (kaynak kod client'a hiç gitmez — Roblox'un yaptığı da tam olarak bu).
- **Gradual typing:** `--!strict` ile tip kontrolü açılabilir, ama bizim için bu öncelik değil; MVP'de dinamik tipleme yeterli.

### A.2 VM başlatma iskeleti

```cpp
// Engine/Scripting/LuauRuntime/LuauVM.h

#pragma once
#include <lua.h>
#include <lualib.h>
#include <Luau/Compiler.h>

class LuauVM {
public:
    void initialize() {
        L = luaL_newstate();
        luaL_openlibs(L);          // Temel kütüphaneler (string, table, math)
        removeUnsafeLibraries();    // ★ Bölüm C'de detaylandırılıyor
        registerEngineAPI();        // ★ Bölüm B'de detaylandırılıyor
    }

    lua_State* getState() const { return L; }

    // Kaynak kodu bytecode'a çevirip çalıştırır
    bool executeScript(const std::string& source, const std::string& chunkName) {
        std::string bytecode = Luau::compile(source);

        if (luau_load(L, chunkName.c_str(), bytecode.data(), bytecode.size(), 0) != 0) {
            logCompileError(lua_tostring(L, -1));
            return false;
        }

        int result = lua_pcall(L, 0, 0, 0);
        if (result != LUA_OK) {
            logRuntimeError(lua_tostring(L, -1));
            return false;
        }
        return true;
    }

private:
    lua_State* L = nullptr;
    void removeUnsafeLibraries();
    void registerEngineAPI();
};
```

---

## Bölüm B — Reflection → Luau Köprüsü

### B.1 Temel strateji: Her sınıf için ayrı glue kod YAZMAYACAĞIZ

Naif bir yaklaşım, her C++ sınıfı için elle bir Luau binding fonksiyonu yazmak olurdu (`lua_pushpartposition`, `lua_setpartposition` gibi onlarca fonksiyon). Bu, Faz 1'de kurduğumuz reflection sisteminin tüm amacını boşa çıkarır. Bunun yerine **tek bir generic metatable** yazıyoruz — bu metatable, hangi C++ sınıfının userdata'sı olursa olsun, reflection sistemine bakarak property okuma/yazmayı otomatik hallediyor.

### B.2 Instance'ı Luau userdata olarak temsil etmek

```cpp
// Engine/Scripting/LuauRuntime/InstanceBinding.cpp

struct InstanceUserdata {
    std::shared_ptr<Instance> instance; // Güçlü referans — Luau bu objeyi hayatta tutar
};

// Bir Instance'ı Luau'ya "push" etmek (C++'tan script'e obje geçirmek)
void pushInstance(lua_State* L, const std::shared_ptr<Instance>& inst) {
    if (!inst) { lua_pushnil(L); return; }

    void* mem = lua_newuserdatatagged(L, sizeof(InstanceUserdata), kInstanceTag);
    new (mem) InstanceUserdata{inst};

    luaL_getmetatable(L, "InstanceMetatable"); // ★ Tek, paylaşılan metatable
    lua_setmetatable(L, -2);
}
```

### B.3 Generic `__index` — property okuma köprüsü

Bu, tüm sistemin kalbi. `part.Position` yazıldığında Luau bu fonksiyonu çağırır:

```cpp
static int instance_index(lua_State* L) {
    InstanceUserdata* ud = static_cast<InstanceUserdata*>(
        lua_touserdatatagged(L, 1, kInstanceTag)
    );
    const char* key = luaL_checkstring(L, 2);

    auto* classDesc = TypeRegistry::instance().find(getClassName(ud->instance));

    // 1. Önce property olarak ara (kalıtım zincirini de tarar — Faz 1'de kurulmuştu)
    if (const PropertyDescriptor* prop = classDesc->findProperty(key)) {
        std::any value = prop->getter(ud->instance.get());
        pushAnyToLuau(L, value, prop->kind); // std::any -> Luau değeri dönüşümü
        return 1;
    }

    // 2. Property değilse, metod olarak ara
    if (const MethodDescriptor* method = classDesc->findMethod(key)) {
        lua_pushlightuserdata(L, (void*)method);
        lua_pushcclosure(L, instance_method_dispatcher, "method", 1);
        return 1;
    }

    // 3. Ne property ne metod — Roblox'taki gibi anlamlı bir hata ver
    luaL_error(L, "'%s' is not a valid member of %s", key, classDesc->className.c_str());
    return 0;
}
```

### B.4 Generic `__newindex` — property yazma köprüsü

```cpp
static int instance_newindex(lua_State* L) {
    InstanceUserdata* ud = static_cast<InstanceUserdata*>(
        lua_touserdatatagged(L, 1, kInstanceTag)
    );
    const char* key = luaL_checkstring(L, 2);

    auto* classDesc = TypeRegistry::instance().find(getClassName(ud->instance));
    const PropertyDescriptor* prop = classDesc->findProperty(key);

    if (!prop) {
        luaL_error(L, "'%s' is not a valid member of %s", key, classDesc->className.c_str());
        return 0;
    }
    if (prop->readOnly) {
        luaL_error(L, "'%s' is read-only", key);
        return 0;
    }

    std::any value = luauValueToAny(L, 3, prop->kind); // Luau değeri -> std::any dönüşümü
    prop->setter(ud->instance.get(), value);
    return 0;
}
```

### B.5 `std::any` ⟷ Luau değeri dönüşümü

Bu, Faz 1.5'te (derinleştirme dokümanında) tanımlanan `PropertyDescriptor::Kind` alanına göre dallanıyor:

```cpp
void pushAnyToLuau(lua_State* L, const std::any& value, PropertyDescriptor::Kind kind) {
    switch (kind) {
        case PropertyDescriptor::Kind::Primitive: {
            if (value.type() == typeid(float))
                lua_pushnumber(L, std::any_cast<float>(value));
            else if (value.type() == typeid(bool))
                lua_pushboolean(L, std::any_cast<bool>(value));
            else if (value.type() == typeid(Vector3))
                pushVector3(L, std::any_cast<Vector3>(value)); // Kendi userdata tipi (Bölüm B.6)
            break;
        }
        case PropertyDescriptor::Kind::Enum: {
            // Enum, script tarafında "Enum.Material.Wood" gibi bir tabloya karşılık gelir
            pushEnumItem(L, value);
            break;
        }
        case PropertyDescriptor::Kind::ObjectRef: {
            auto ref = std::any_cast<std::shared_ptr<Instance>>(value);
            pushInstance(L, ref); // Recursive — başka bir Instance da aynı köprüden geçer
            break;
        }
        // Kind::Array için benzer mantık, Luau tablosuna dönüştürülür
    }
}
```

### B.6 Vector3 gibi "değer tipleri" için ayrı, hafif bir binding

`Instance` gibi referans tipler userdata + shared_ptr ile temsil edilirken, `Vector3` gibi value type'lar için farklı, daha hafif bir yaklaşım kullanılıyor — çünkü her `Vector3.new()` çağrısında bir `shared_ptr` oluşturmak gereksiz overhead olur:

```cpp
struct Vector3Userdata { float x, y, z; };

static int vector3_add(lua_State* L) {
    Vector3Userdata* a = checkVector3(L, 1);
    Vector3Userdata* b = checkVector3(L, 2);
    pushVector3(L, {a->x + b->x, a->y + b->y, a->z + b->z});
    return 1;
}

// Metatable kaydı — Vector3'e özel, Instance'tan bağımsız
void registerVector3Type(lua_State* L) {
    luaL_newmetatable(L, "Vector3Metatable");
    lua_pushcfunction(L, vector3_add, "__add");
    lua_setfield(L, -2, "__add");   // a + b çalışsın diye operatör overload
    lua_pushcfunction(L, vector3_index, "__index");
    lua_setfield(L, -2, "__index"); // v.X, v.Y, v.Z okunabilsin diye
    lua_pop(L, 1);
}
```

**Bu ayrımın nedeni:** `Instance` türevleri (Part, Script, vb.) kalıcı, kimlikli nesnelerdir — bellekte bir kez var olur, referanslarla paylaşılır. `Vector3` ise matematiksel bir değerdir, her işlemde yenisi oluşur (`pos + Vector3.new(0,1,0)` gibi). İkisini aynı mekanizmayla (reflection + shared_ptr) yönetmeye çalışmak hem performans kaybı hem gereksiz karmaşıklık yaratırdı.

---

## Bölüm C — Sandboxing / Güvenlik

### C.1 Neden gerekli?

Bir `Script` içindeki kod, hem sunucuda hem (Roblox modelinde) istemcide çalışacak. Bir script'in dosya sistemine yazması, ağa keyfi bağlantı açması, ya da `require`'la sistem kütüphanelerine erişmesi **kabul edilemez** — bu bir güvenlik açığıdır (özellikle multiplayer bir ortamda, oyuncuların yazdığı script'ler diğer oyuncuları etkileyebiliyorsa).

### C.2 Tehlikeli kütüphanelerin kaldırılması

```cpp
void LuauVM::removeUnsafeLibraries() {
    // io, os, debug, package gibi kütüphaneler TAMAMEN kaldırılıyor
    lua_pushnil(L); lua_setglobal(L, "io");
    lua_pushnil(L); lua_setglobal(L, "os");       // Sadece os.time gibi zararsız
    lua_pushnil(L); lua_setglobal(L, "package");   // Dosyadan keyfi modül yükleme YOK
    lua_pushnil(L); lua_setglobal(L, "dofile");
    lua_pushnil(L); lua_setglobal(L, "loadfile");

    // os.time gibi zararsız alt fonksiyonları geri, kontrollü şekilde ekliyoruz
    lua_newtable(L);
    lua_pushcfunction(L, safe_os_time, "time");
    lua_setfield(L, -2, "time");
    lua_setglobal(L, "os");
}
```

### C.3 Her Script'e izole bir global ortam (environment) verilmesi

Roblox'ta bir Script'teki `x = 5` yazımı, başka bir Script'in görebileceği global bir değişken **oluşturmaz** — her Script kendi izole ortamında çalışır. Bunu Luau'da şu şekilde sağlıyoruz:

```cpp
// Her Script çalıştırılmadan önce kendi environment tablosunu alır
void executeScriptIsolated(lua_State* L, const std::string& bytecode, Script* scriptInstance) {
    lua_State* thread = lua_newthread(L); // ★ Her script kendi coroutine'inde çalışır

    lua_newtable(thread);              // Script'e özel boş environment
    lua_newtable(thread);              // Bu environment'ın metatable'ı
    lua_getglobal(thread, "_G");
    lua_setfield(thread, -2, "__index"); // Bilinmeyen okuma -> global _G'ye düşer (paylaşılan API'ler için)
    lua_setmetatable(thread, -2);

    lua_setfenv(thread, -1); // Bu thread'in "global tablosu" artık izole edilmiş tablo

    luau_load(thread, scriptInstance->name.c_str(), bytecode.data(), bytecode.size(), 0);
    lua_resume(thread, L, 0);
}
```

**Bu tasarımın anlamı:** İki farklı `Script`, aynı isimde global değişken tanımlasa bile birbirini etkilemez (Roblox'taki davranış budur). Ama her ikisi de `workspace`, `Vector3.new` gibi motor API'lerine erişebilir çünkü bunlar paylaşılan `_G` üzerinden `__index` zinciriyle görünür kalır.

---

## Bölüm D — Event Sistemi (Signal/Connection)

### D.1 Neden özel bir sistem gerekiyor?

Roblox'taki `part.Touched:Connect(function(hit) ... end)` deseni, C++ tarafında bir event/callback sistemine karşılık gelir. Bunu hem C++ dünyasında (bir Part diğerine çarptığında fizik motoru bir şey tetiklemeli) hem Luau dünyasında (script bu tetiklemeyi dinleyebilmeli) çalışacak şekilde tasarlamamız gerekiyor.

```cpp
// Engine/Core/Signal.h

class Signal {
public:
    struct Connection {
        uint32_t id;
        std::function<void(std::vector<std::any>)> callback;
    };

    uint32_t connect(std::function<void(std::vector<std::any>)> cb) {
        uint32_t id = nextId++;
        connections.push_back({id, std::move(cb)});
        return id;
    }

    void disconnect(uint32_t id) {
        connections.erase(
            std::remove_if(connections.begin(), connections.end(),
                [id](auto& c) { return c.id == id; }),
            connections.end()
        );
    }

    void fire(std::vector<std::any> args) {
        for (auto& conn : connections) conn.callback(args); // C++ dinleyicileri
    }

private:
    std::vector<Connection> connections;
    uint32_t nextId = 0;
};
```

### D.2 Luau tarafına köprü — `:Connect()` metodunun çalışması

`part.Touched` bir Luau'dan erişildiğinde, aslında bir "Signal wrapper" userdata döner. Bunun `Connect` metodu çağrıldığında, verilen Luau fonksiyonu bir C++ `std::function`'a sarılıp `Signal::connect()`'e verilir:

```cpp
static int signal_connect(lua_State* L) {
    Signal* signal = checkSignal(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Luau fonksiyonunu referans olarak sakla (garbage collector'ın silmemesi için)
    int funcRef = lua_ref(L, 2);
    lua_State* mainThread = getMainThread(L);

    uint32_t connId = signal->connect([mainThread, funcRef](std::vector<std::any> args) {
        lua_getref(mainThread, funcRef);
        for (auto& arg : args) pushAnyToLuau(mainThread, arg, inferKind(arg));
        lua_pcall(mainThread, (int)args.size(), 0, 0); // Luau callback'i çağrılıyor
    });

    pushConnectionObject(L, signal, connId); // :Disconnect() çağrılabilsin diye
    return 1;
}
```

Fizik sistemi (Faz 5) bir çarpışma tespit ettiğinde sadece şunu çağırması yeterli olacak:

```cpp
part->touchedSignal.fire({otherPart});
```

Bu tek satır, hem C++ dinleyicilerini hem (eğer varsa) Luau script'lerindeki `:Connect()` ile bağlanmış fonksiyonları otomatik tetikleyecek.

---

## Bölüm E — Script Çalıştırma Modeli: `wait()` ve Coroutine'ler

### E.1 Sorun: Bir script `wait(2)` dediğinde tüm motor donmalı mı?

Kesinlikle hayır. Roblox'ta bir script `wait(2)` çağırdığında sadece o script 2 saniye "duraklar", diğer script'ler ve render döngüsü çalışmaya devam eder. Bu, Bölüm C.3'te zaten kurduğumuz **her Script'in kendi coroutine'inde çalışması** kararı sayesinde doğal olarak çözülüyor.

```cpp
static int luau_wait(lua_State* L) {
    double duration = luaL_optnumber(L, 1, 0.0);
    ScriptScheduler::instance().suspendCurrentThread(L, duration);
    return lua_yield(L, 0); // ★ Coroutine'i "duraklat", motoru DONDURMA
}
```

### E.2 Ana motor döngüsü, bekleyen script'leri nasıl uyandırıyor?

```cpp
// Her karede (Renderer::renderFrame'den önce) çağrılır
void ScriptScheduler::update(float deltaTime) {
    for (auto it = suspendedThreads.begin(); it != suspendedThreads.end(); ) {
        it->remainingTime -= deltaTime;
        if (it->remainingTime <= 0.0f) {
            lua_resume(it->thread, nullptr, 0); // Script kaldığı yerden devam eder
            it = suspendedThreads.erase(it);
        } else {
            ++it;
        }
    }
}
```

**Bu tasarımın önemi:** Motor her karede binlerce script'i "tek seferde" çalıştırmıyor — sadece o karede uyanması gereken script'leri resume ediyor. Bu, Roblox'un da kullandığı kooperatif çoklu görev (cooperative multitasking) modelidir; işletim sistemi seviyesinde thread açmaktan çok daha hafiftir.

---

## Bölüm F — Uçtan Uca Örnek: `part.Position = Vector3.new(0,10,0)` satırının tam yolculuğu

1. Luau derleyicisi bu satırı bytecode'a çevirir: `GETGLOBAL part`, `NEWTABLE` (Vector3.new çağrısı), `SETINDEX`.
2. `Vector3.new(0,10,0)` çalışır → Bölüm B.6'daki `Vector3Userdata` oluşturulur.
3. `part.Position = ...` ifadesi Luau VM'inde `__newindex` metamethod'unu tetikler → Bölüm B.4'teki `instance_newindex` çalışır.
4. `instance_newindex`, reflection sisteminden (Faz 1) `"Position"` property'sini bulur.
5. Luau'daki `Vector3Userdata`, C++'taki `Vector3` struct'ına dönüştürülür (`luauValueToAny`).
6. `PropertyDescriptor::setter` çağrılır → gerçek C++ kodu çalışır: `part->position = newValue`.
7. `Part::setPosition` (Faz 2'de tanımlı) tetiklenir → `RenderProxy` dirty işaretlenir.
8. Bir sonraki karede Renderer, bu dirty proxy'yi görüp transform'u günceller ve obje ekranda yeni konumunda belirir.

Bu zincir, üç ayrı fazda (1, 2, 3) tasarlanan sistemlerin nasıl **tek bir kayıt noktasından** (reflection) beslenerek birbirine kusursuz bağlandığını gösteriyor.

---

## Bölüm G — Faz 3 "Definition of Done" Kontrol Listesi

- [ ] Luau VM başlatılıyor, tehlikeli kütüphaneler (`io`, `os.execute`, `package`) erişilemez durumda
- [ ] Generic `__index`/`__newindex` üzerinden, **hiçbir sınıfa özel glue kod yazmadan**, en az 3 farklı Instance türevinin (Part, Model, Script) property'lerine erişilebiliyor
- [ ] `Vector3.new(x,y,z)` ve temel operatörler (`+`, `-`, `*`) çalışıyor
- [ ] İki farklı Script aynı isimde global değişken tanımladığında birbirini etkilemiyor (izolasyon testi)
- [ ] `part.Touched:Connect(function(hit) ... end)` çalışıyor ve C++ tarafından `fire()` çağrıldığında Luau fonksiyonu tetikleniyor
- [ ] `wait(2)` bir script'i durdurduğunda, **diğer script'ler ve render döngüsü durmuyor** (paralel çalışma testiyle doğrulanmalı)
- [ ] Geçersiz bir property'ye erişim (`part.Wrongname`) anlamlı bir hata mesajıyla script'i durduruyor, motoru çökertmiyor
- [ ] Script içinde sonsuz döngü (`while true do end`) yazıldığında motor tamamen donmuyor (bir "script timeout" / instruction count limiti mekanizması gerekebilir — bu, güvenlik açısından ayrıca ele alınmalı)

---

## Sonraki Adım Önerisi

Faz 3 tamamlandığında elimizde: obje oluşturabilen, script yazabilen, event dinleyebilen çalışan bir "sandbox" var. Sıradaki mantıklı adımlar:

1. **Faz 4 — Editör:** Bu sistemlerin üzerine ImGui tabanlı Explorer/Properties/Viewport panellerinin kurulması (bu noktada gerçek anlamda "Studio" deneyimi başlıyor)
2. **Faz 5'in öne çekilmesi — Fizik:** `Touched` event'inin gerçekten tetiklenebilmesi için Jolt Physics entegrasyonu
3. **Script timeout / instruction limiti konusunun derinleştirilmesi:** Yukarıdaki kontrol listesindeki son madde, güvenlik açısından kritik ve ayrı bir teknik tartışma gerektiriyor

Hangisiyle devam edelim?
