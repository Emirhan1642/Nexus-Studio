# Script Timeout ve Sonsuz Döngü Koruması (Aşama 2b)

Bu plan, Luau script'lerinde (örneğin `while true do end`) yaşanabilecek sonsuz döngülerin tüm motoru (render, fizik, networking) dondurmasını önlemek için geliştirilmiştir.

## User Review Required

Bu plan, `Script_Timeout_Sonsuz_Dongu_Korumasi.md` tasarım dokümanına dayanmaktadır. 

> [!IMPORTANT]
> Script bütçesi aşıldığında script doğrudan hata (Error) fırlatarak öldürülecektir (Roblox yaklaşımı). 
> İleri seviye "Editor Plugin" diyaloğu (Devam Et/İptal Et) altyapısı kurulacak, ancak Editor Plugin sistemi tam aktif olana kadar varsayılan olarak Hard-Abort uygulanacaktır.

## Open Questions

> [!WARNING]
> Şu anki Luau entegrasyonumuzda her script `LuauVM::executeScript()` içinden `lua_resume` ile doğrudan başlatılıyor ve daha sonra `ScriptScheduler` üzerinden `wait()` sonrasında devam ettiriliyor. Bütçe aşımı sonrası script'in tamamen Scheduler'dan çıkarılması (dead/errored state) mantığını uygulayacağız, onaylıyor musunuz?

## Proposed Changes

---

### Engine/Scripting/LuauRuntime

#### [NEW] [ScriptWatchdog.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptWatchdog.h)
- `ScriptExecutionPhase` enum'u (`Heartbeat`, `Initialization`, vs.) tanımlanacak.
- `ScriptExecutionContext` yapısı oluşturularak bütçe (max instruction, max süre) takibi yapılacak.
- `ScriptWatchdog` sınıfı tanımlanıp, statik `install(lua_State* L)` fonksiyonuyla `lua_callbacks(L)->interrupt` callback'i bağlanacak.

#### [NEW] [ScriptWatchdog.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptWatchdog.cpp)
- `ScriptWatchdog::onInterrupt` callback'i yazılacak.
- Aktif script'in ne kadar zamandır çalıştığı `std::chrono::steady_clock` ile ölçülecek.
- Eğer bütçe (örn. 8ms veya 500,000 instruction) aşılırsa `luaL_error(L, "Script execution budget exceeded")` çağrılarak VM'in durması sağlanacak.

#### [MODIFY] [LuauVM.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/LuauVM.cpp)
- `LuauVM::init()` içerisinde `ScriptWatchdog::install(L)` çağrılacak.
- `LuauVM::executeScript` içerisinde ilk çalıştırma (Initialization) sırasında `ScriptExecutionContext` oluşturulup aktif context olarak atanacak. Hata durumunda Output'a loglanacak.

#### [MODIFY] [ScriptScheduler.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.cpp)
- `ScriptScheduler::update()` içerisindeki `lua_resume` döngüsünde, her resume işleminden önce `ScriptExecutionContext` Heartbeat bütçesiyle atanacak.
- `lua_resume` hata döndürdüğünde (Timeout nedeniyle `luaL_error` tetiklenirse) script ölü kabul edilecek ve hata konsola/output'a yazılacak.

#### [MODIFY] [CMakeLists.txt](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/CMakeLists.txt)
- `LuauRuntime/ScriptWatchdog.cpp` CMake dosyasına eklenecek.

## Verification Plan

### Automated Tests
- Test yazmaya uygunsa `NexusStudioTests` içerisine sonsuz döngülü bir script ekleyip motorun çökmediğini doğrulayacağız.

### Manual Verification
- Bir `Part` içerisine sonsuz döngülü (`while true do end`) bir script ekleyip Play modunu (F5) başlatacağız.
- Editörün ve fizik motorunun kilitlenmediğini, Output'ta "Script execution budget exceeded" hatasının göründüğünü teyit edeceğiz.
