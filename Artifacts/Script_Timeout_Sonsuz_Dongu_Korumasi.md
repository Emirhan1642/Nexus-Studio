# Teknik Derinlemesine İnceleme — Script Timeout ve Sonsuz Döngü Koruması

Bu doküman, Faz 3'ten beri ertelenen ama Faz 4 (editör donması riski), Faz 5 (fizik adımının kilitlenmesi) ve Faz 6'da (bir script'in tüm sunucuyu ve ona bağlı oyuncuları etkileme riski) önceliği sürekli artan bir konuyu ele alıyor: **Bir script `while true do end` yazdığında ne olacak?**

---

## Bölüm A — Sorunun Gerçek Boyutu

### A.1 Neden bu, "sonradan eklenebilecek" bir özellik değil

Script Scheduler'ı (Faz 3, Bölüm E) hatırlayalım: her Script kendi coroutine'inde (Luau thread'inde) çalışıyor ve `wait()` çağrıldığında `lua_yield` ile kontrolü ana döngüye bırakıyor. Ama bu mekanizma **sadece script'in kendi isteğiyle** (`wait()` çağırdığında) işliyor. Eğer script hiç `wait()` çağırmadan sonsuz bir döngüye girerse (`while true do x = x + 1 end`), `lua_resume()` çağrısı **asla geri dönmez** — çünkü Lua/Luau VM'i, tek bir `lua_resume` çağrısı içinde çalışırken kesintiye uğratılamaz (preemptible değildir). Bu, ana thread'i tamamen kilitler: render durur, fizik durur, ağ paketleri işlenmez, editör donar.

Bu, "nadir bir edge case" değil — bir motorun editörünü kullanıcılara açtığın anda **kesinlikle olacak** bir senaryodur (kasıtlı kötü niyet olmasa bile, yeni başlayan bir geliştirici yanlışlıkla sonsuz döngü yazabilir).

### A.2 Neden basit bir "ayrı thread'de çalıştır, N saniye sonra kill et" çözümü yetersiz

İlk akla gelen naif çözüm: her script'i işletim sistemi seviyesinde ayrı bir thread'de çalıştırmak, zaman aşımında `pthread_cancel` veya benzeri ile öldürmek.

Bu yaklaşım ciddi sorunlar taşıyor:
- Lua/Luau VM'i **thread-safe değildir** — aynı `lua_State`'e birden fazla OS thread'inin eş zamanlı erişmesi veri bozulmasına yol açar (her script için ayrı bir `lua_State` açmak mümkün ama bu, DataModel'e erişimi de thread-safe hale getirmeyi gerektirir — bu da Faz 5'teki fizik thread'i sorununun onlarca kez katlanmış hali olurdu).
- OS thread'ini zorla öldürmek (`TerminateThread` / `pthread_cancel`), o thread'in tuttuğu kilitleri (mutex) serbest bırakmadan sonlandırabilir — bu da deadlock veya bozuk state'e yol açar.
- Yüzlerce/binlerce script için yüzlerce/binlerce OS thread açmak, işletim sistemi kaynaklarını (stack bellek, context-switch maliyeti) gereksiz tüketir.

**Bu yüzden doğru çözüm, OS thread seviyesinde değil, Luau VM'inin kendi içindeki bir mekanizma olmalı.**

---

## Bölüm B — Çözüm: Luau'nun Yerleşik Interrupt Mekanizması

### B.1 Luau'nun standart Lua'dan bu konudaki kritik farkı

Luau, tam olarak bu senaryo için tasarlanmış bir özellik sunuyor (Roblox'un kendi production ortamında binlerce eşzamanlı, güvenilmeyen script çalıştırma ihtiyacından doğmuş): `lua_callbacks(L)->interrupt` callback'i. Bu callback, VM'in çalıştırdığı **her N bytecode instruction'da bir** otomatik olarak tetikleniyor — script'in kendi isteği dışında, VM'in kendisi tarafından.

```cpp
// Engine/Scripting/LuauRuntime/ScriptWatchdog.h

#pragma once
#include <lua.h>
#include <chrono>

class ScriptWatchdog {
public:
    static void install(lua_State* L) {
        lua_Callbacks* callbacks = lua_callbacks(L);
        callbacks->interrupt = onInterrupt; // ★ Her instruction batch'inde çağrılır
    }

private:
    static void onInterrupt(lua_State* L, int gc) {
        if (gc != -1) return; // gc parametresi -1 değilse bu bir GC callback'i, script instruction'ı değil

        ScriptExecutionContext* ctx = getCurrentExecutionContext(L);
        auto elapsed = std::chrono::steady_clock::now() - ctx->startTime;

        if (elapsed > ctx->budget) {
            // ★ Kritik: burada script'i "öldürmüyoruz", VM'e bir hata fırlatmasını söylüyoruz
            luaL_error(L, "Script execution budget exceeded (%s)", ctx->scriptName.c_str());
        }
    }
};
```

**Bu mekanizmanın neden güvenli olduğu:** `luaL_error()` çağrısı, C++ `throw` gibi davranır ama Lua'nın kendi hata yönetim mekanizması (longjmp tabanlı) üzerinden çalışır — VM'in kendi stack'ini düzgünce temizleyerek geri sarar. Hiçbir mutex kilitli kalmaz, hiçbir bellek sızmaz, çünkü Luau bu senaryo için baştan tasarlanmış.

### B.2 Instruction sayısı mı, gerçek zaman mı?

İki yaklaşım var ve ikisinin de kullanım yeri farklı:

| Yaklaşım | Ne ölçer | Avantaj | Dezavantaj |
|---|---|---|---|
| Instruction count budget | Kaç bytecode komutu çalıştı | Deterministik, platformdan bağımsız | Bir instruction'ın gerçek maliyeti değişken (örn. bir C++ fonksiyon çağrısı 1 instruction ama pahalı olabilir) |
| Wall-clock time budget | Gerçek geçen süre | Kullanıcı deneyimine doğrudan bağlı ("editör 1 saniyeden fazla donmasın") | Platformdan platforma, yükten yüke değişken |

**Karar: İkisinin birleşimi.** Interrupt callback'i her çağrıldığında hem instruction sayacı hem gerçek zaman kontrol ediliyor — hangisi önce aşılırsa script durduruluyor:

```cpp
static void onInterrupt(lua_State* L, int gc) {
    if (gc != -1) return;

    ScriptExecutionContext* ctx = getCurrentExecutionContext(L);
    ctx->instructionCount += INTERRUPT_GRANULARITY; // Luau, her ~N instruction'da bir interrupt çağırır

    bool overInstructionBudget = ctx->instructionCount > ctx->maxInstructions;
    bool overTimeBudget = (std::chrono::steady_clock::now() - ctx->startTime) > ctx->maxDuration;

    if (overInstructionBudget || overTimeBudget) {
        luaL_error(L, "Script '%s' exceeded execution budget (%s)",
            ctx->scriptName.c_str(),
            overTimeBudget ? "time limit" : "instruction limit");
    }
}
```

---

## Bölüm C — Bütçe (Budget) Ne Olmalı? Bağlama Göre Farklı Stratejiler

### C.1 Tek bir global sınır yeterli değil

Bir script'in çalışma bütçesi, **hangi bağlamda çalıştığına** göre değişmeli:

```cpp
enum class ScriptExecutionPhase {
    Heartbeat,      // Her karede bir kez çalışan normal oyun mantığı — SIKI bütçe
    Initialization, // Script ilk yüklendiğinde çalışan kurulum kodu — biraz daha gevşek
    RemoteEventCallback, // Bir RemoteEvent tetiklendiğinde — orta sıkılıkta
    EditorPluginCode,    // Editör içinde çalışan araç kodu — kullanıcı bekleyebilir, gevşek
};

struct ExecutionBudget {
    int maxInstructions;
    std::chrono::milliseconds maxDuration;
};

ExecutionBudget getBudgetFor(ScriptExecutionPhase phase) {
    switch (phase) {
        case ScriptExecutionPhase::Heartbeat:
            return {500'000, std::chrono::milliseconds(8)}; // 60 FPS'te bir karenin payı ~16ms, script'e 8ms
        case ScriptExecutionPhase::Initialization:
            return {5'000'000, std::chrono::milliseconds(1000)};
        case ScriptExecutionPhase::RemoteEventCallback:
            return {1'000'000, std::chrono::milliseconds(50)};
        case ScriptExecutionPhase::EditorPluginCode:
            return {50'000'000, std::chrono::milliseconds(5000)};
    }
}
```

**Gerekçe:** Her karede çalışan bir Heartbeat script'i, 8ms'den fazla sürerse frame rate'i doğrudan düşürür — bu yüzden çok sıkı bir bütçesi olmalı. Ama script ilk yüklenirken (`Initialization`) büyük bir tabloyu doldurmak gibi meşru, tek seferlik ağır işler olabilir — bu yüzden daha gevşek.

### C.2 Sunucu bağlamında ekstra hassasiyet (Faz 6 ile ilişki)

Faz 6'da kurduğumuz sunucu-otoriteli mimaride, **sunucu tek bir process içinde potansiyel olarak yüzlerce oyuncuya hizmet veriyor.** Bir script'in sunucuda kilitlenmesi, o script'i yazan kişiyi değil, **sunucudaki herkesi** etkiler. Bu yüzden sunucu tarafında bütçeler istemciye göre daha da sıkı tutulmalı, ve üstüne bir "toplam kare bütçesi" eklenmeli:

```cpp
class ServerFrameBudgetGuard {
public:
    void beginFrame() { totalScriptTimeThisFrame = std::chrono::milliseconds(0); }

    bool hasRemainingBudget() const {
        return totalScriptTimeThisFrame < MAX_TOTAL_SCRIPT_TIME_PER_FRAME; // örn. 10ms
    }

    void recordScriptTime(std::chrono::milliseconds elapsed) {
        totalScriptTimeThisFrame += elapsed;
    }

private:
    std::chrono::milliseconds totalScriptTimeThisFrame{0};
    static constexpr auto MAX_TOTAL_SCRIPT_TIME_PER_FRAME = std::chrono::milliseconds(10);
};
```

Bu koruma, tek bir "yavaş ama sonsuz döngüye girmeyen" script'in bile, o karede çalışması planlanan **diğer** script'leri erteleyerek (throttling) toplam kare süresinin kontrolden çıkmasını engelliyor.

---

## Bölüm D — Hata Sonrası Ne Olur? Script'in Durumu

### D.1 Budget aşıldığında script tamamen mi ölüyor, yoksa bir sonraki karede devam mı ediyor?

Bu, ürün kararı gerektiren bir noktadır. İki seçenek:

**Seçenek 1 — Script tamamen sonlandırılır (Roblox'un yaklaşımı sonsuz döngüler için):** `while true do end` gibi gerçek bir sonsuz döngü tespit edildiğinde, script bir daha çalıştırılmaz, Output/Console panelinde kırmızı bir hata gösterilir.

**Seçenek 2 — Script bir sonraki karede "kaldığı yerden" devam eder:** Bu, meşru ama uzun süren işler (örn. büyük bir prosedürel dünya üretimi) için daha uygun — ama bu, script'in **kendi isteğiyle** ara ara `wait()` veya `task.wait()` çağırmasını gerektirir, yoksa zaten yakalanacağı ilk duruma geri döner.

**Karar: Heartbeat/RemoteEvent bağlamında Seçenek 1 (sonlandır), Initialization bağlamında da Seçenek 1 ama daha gevşek bütçeyle.** Gerçek anlamda uzun süren meşru işler için geliştiriciye **kendi script'ini `task.wait()` ile bölme sorumluluğu** yükleniyor — bu, Roblox'un da benimsediği modeldir ve motor tarafında "akıllı otomatik bölme" gibi çözülmesi imkânsız bir problemi geliştiriciye doğru şekilde devrediyor.

### D.2 Hatanın Editöre / Output paneline yansıması

```cpp
// Engine/Scripting/LuauRuntime/ScriptExecutor.h

void ScriptExecutor::resumeScript(ScriptInstance* script) {
    ScriptExecutionContext ctx{script->name, std::chrono::steady_clock::now(), getBudgetFor(currentPhase)};
    setCurrentExecutionContext(script->luauThread, &ctx);

    int status = lua_resume(script->luauThread, nullptr, 0);

    if (status != LUA_OK && status != LUA_YIELD) {
        std::string errorMsg = lua_tostring(script->luauThread, -1);

        // ★ Faz 4'teki Output/Console paneline gönderilir (henüz varsa; yoksa bu fazda eklenir)
        EditorOutputPanel::instance().logError(script->name, errorMsg);

        // ★ Script'in coroutine'i artık "ölü" (dead) durumda — bir daha resume edilmeyecek
        script->state = ScriptInstance::State::Errored;

        // Sunucu bağlamındaysa, bu bilgi geliştiriciye ayrıca (örn. bir log dosyasına) da yazılmalı
        if (NetworkContext::mode() == NetworkMode::Server) {
            ServerLog::instance().writeError(script->name, errorMsg);
        }
    }
}
```

**Önemli tasarım kararı:** Bir script hata verip durduğunda, **diğer script'ler etkilenmemeli.** Her script kendi coroutine'inde izole çalıştığı için (Faz 3, Bölüm C.3), bir script'in çökmesi doğal olarak diğerlerini etkilemiyor — bu, Faz 3'teki izolasyon kararının burada da faydasını gösteren bir nokta.

---

## Bölüm E — Editöre Özel Ek Önlem: "Are You Sure?" Diyalogu

### E.1 Editor Plugin bağlamında bütçe neden gevşek olabiliyor, ama sınırsız olmamalı

Bölüm C.1'de `EditorPluginCode` bağlamı için gevşek bir bütçe (5 saniye) tanımlanmıştı. Ama editörde çalışan bir araç scripti (örn. "sahnedeki tüm ağaçları rastgele dağıt" gibi bir plugin) meşru olarak birkaç saniye sürebilir. Bu durumda script'i sert bir şekilde kesmek yerine, kullanıcıya seçenek sunmak daha iyi bir deneyim:

```cpp
static void onInterrupt(lua_State* L, int gc) {
    if (gc != -1) return;
    ScriptExecutionContext* ctx = getCurrentExecutionContext(L);

    if (ctx->phase == ScriptExecutionPhase::EditorPluginCode) {
        auto elapsed = std::chrono::steady_clock::now() - ctx->startTime;
        if (elapsed > std::chrono::milliseconds(3000) && !ctx->warningShown) {
            ctx->warningShown = true;
            // ★ Editör thread'ine (ImGui) bir modal göstermesi için sinyal gönderilir
            //   (VM'i durdurmadan önce kullanıcıya "devam etsin mi?" sorulur)
            EditorApp::instance().requestLongRunningScriptConfirmation(ctx->scriptName);
        }
        if (elapsed > ctx->maxDuration && !EditorApp::instance().userAllowedContinuation(ctx->scriptName)) {
            luaL_error(L, "Script cancelled by user (long-running operation)");
        }
        return;
    }
    // ... diğer fazlar için Bölüm B.2'deki mantık
}
```

Bu, tam olarak Excel/Word gibi masaüstü uygulamalarının "Bu makro uzun sürüyor, iptal etmek ister misiniz?" diyaloğuna benzer bir kullanıcı deneyimi.

---

## Bölüm F — Fizik ve Ağ Thread'leriyle Etkileşim (Faz 5 ve Faz 6 ile bağlantı)

### F.1 Script'in kilitlenmesi, Faz 5'teki fizik senkronizasyonunu nasıl etkiler?

Faz 5'te ana motor döngüsü şu sırayı izliyordu: `PhysicsWorld::step()` → `processPhysicsEvents()` (Touched event'leri kuyruktan boşaltma) → script güncellemeleri. Eğer bir Touched event handler'ı sonsuz döngüye girerse, artık **watchdog bu döngüyü kesiyor**, ama kesilene kadar geçen süre (bütçe kadar, örn. 8ms) o karede fizik adımının bir sonraki karede geç başlamasına neden olur. Bu kabul edilebilir bir gecikme — asıl önemli olan, watchdog olmadan bu sürenin **sonsuz** olacağı senaryonun artık engellenmiş olması.

### F.2 Sunucu tarafında toplu etkilenme senaryosu

Bir script sunucuda sonsuz döngüye girip watchdog tarafından kesildiğinde, o script'i çalıştıran oyuncunun deneyimi bozulmuyor demek değil — o karede sunucunun 8ms'lik bütçesi tükendiği için **diğer** oyuncuların da o karesi gecikmiş olabilir. Bölüm C.2'deki `ServerFrameBudgetGuard`, bu etkiyi sınırlıyor ama sıfıra indiremiyor — bu, dağıtık/multi-tenant bir sunucu mimarisinin doğal bir sınırlaması olarak kabul edilmeli. İleri bir optimizasyon olarak (bu dokümanın kapsamı dışında) her oyuncunun script'lerini ayrı bir "fiber" havuzunda zaman dilimleme (time-slicing) ile çalıştırmak düşünülebilir.

---

## Bölüm G — Definition of Done Kontrol Listesi

- [ ] `lua_callbacks->interrupt` her Luau state'inde kayıtlı — hem sunucu hem istemci hem editör modunda
- [ ] `while true do end` yazan bir Heartbeat script'i, 8ms bütçe aşıldığında düzgün bir hata ile durduruluyor, **motor çökmüyor**
- [ ] Hata mesajı Output/Console panelinde script adı ve satır numarasıyla görünüyor
- [ ] Bir script hata verip durduğunda, sahnedeki **diğer** script'ler etkilenmeden çalışmaya devam ediyor (izolasyon testi)
- [ ] `ScriptExecutionPhase`'e göre farklı bütçeler doğru uygulanıyor (Heartbeat sıkı, Initialization gevşek test edilmiş)
- [ ] `ServerFrameBudgetGuard` çalışıyor — birden fazla script aynı karede toplamda bütçeyi aştığında, sonrakiler bir sonraki kareye ertelenip motor kilitlenmiyor
- [ ] Editör plugin bağlamında 3 saniyeyi geçen bir script için kullanıcıya "devam et/iptal et" diyaloğu çıkıyor
- [ ] Watchdog'un kendisi, script'in `wait()` ile kendi isteğiyle yield olduğu normal senaryolarda **yanlışlıkla** tetiklenmiyor (yani `wait(10)` çağıran meşru bir script hatayla durdurulmuyor — çünkü `lua_yield` zaten kontrolü düzgünce bırakıyor, interrupt callback'i bu durumda devrede değil)
- [ ] Stres testi: 500+ script'in aynı anda çalıştığı bir sahnede, birkaçının kasıtlı olarak sonsuz döngüye sokulduğu bir senaryoda genel FPS/tick rate kabul edilebilir seviyede kalıyor

---

## Sonraki Adım Önerisi

Bu konu artık teknik borç olmaktan çıktı. Faz 3-6 arasında bekleyen üç konudan ikisi hâlâ önümüzde:

1. **Faz 8 — C# desteği:** Sırada bekleyen bir sonraki faz.
2. **Karakter kontrolcüsü (Humanoid sistemi):** Fizik + networking + artık script güvenliği de hazır olduğuna göre, oynanabilirlik tarafına geçmek için uygun zaman.
3. **Faz 4'te bahsedilen Asset Browser / import akışı:** Hâlâ ele alınmadı.

Hangisiyle devam edelim?
