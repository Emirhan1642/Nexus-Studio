# Aşama 3: Luau Scripting & Entegrasyon (Tamamlandı)
- [x] **Task 1:** ThirdParty kütüphanesi olarak `luau`'yu indir ve CMake'e bağla.
- [x] **Task 2:** `Engine/Core/Signal.h` (Generic Event sistemi) oluştur.
- [x] **Task 3:** `LuauRuntime` namespace'i altında `LuauVM` (singleton) sınıfı oluştur ve temel VM ilklendirmelerini (sandboxing dahil) yap.
- [x] **Task 4:** `ScriptScheduler` oluştur ve coroutine tabanlı `wait()` fonksiyonunu C++ ve Lua arasında bağla.
- [x] **Task 5:** `InstanceBinding` ve `Vector3Binding` yazarak `Reflection` (TypeRegistry) üzerinden dinamik özellik (property) okuma/yazma desteğini ekle. `DataModel` içerisine `Script` sınıfını ekle.
- [x] **Task 6:** Editor entegrasyonu ve test (Editor/Main.cpp içerisine `LuauVM::instance().init()`, ana döngüye `ScriptScheduler::instance().update()` ve test script objesi ekleme).

### Sonraki Adımlar (Aşama 4: Editor)
- [ ] ImGui entegrasyonu ve Editor UI oluşturulması.
