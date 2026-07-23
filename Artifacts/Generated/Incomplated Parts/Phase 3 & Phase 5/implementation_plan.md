# Yarım Kalan Görevlerin Tamamlanması (Faz 3 & Faz 5)

Bu plan, Luau için eksik kalan `Connect` (Signal bağlama) özelliğini ve Fizik motorundaki kopuk olan `Touched` çarpışma olayını DataModel/Luau'ya bağlamayı hedefler.

## Open Questions

> [!NOTE]
> Faz 5'te eksik olan **Constraints (Menteşe, Yay vb.)** konusu kapsamlı bir alt sistemdir. İlk aşamada sadece `Touched` ve `Connect` kısımlarına odaklanıp, mekaniksel kısıtlamaları (Constraints) ayrı bir aşamada ele almayı öneriyorum. Bu plana Constraints'i dahil etmeli miyiz, yoksa sonraya mı bırakalım?

## Proposed Changes

---

### Motor Çekirdeği (Core & Reflection)

#### [MODIFY] `Engine/Core/Reflection/TypeRegistry.h`
- `ClassDescriptor` yapısına `SignalDescriptor` veya özel tip olarak Signal dönüşü eklenecek, böylece bir objenin sinyalleri (eventleri) string tabanlı olarak bulunabilecek.

#### [MODIFY] `Engine/Core/Reflection/ClassBuilder.h`
- `signal()` isminde bir metod eklenecek. Örnek kullanım: `.signal("Touched", &Part::Touched)`

#### [MODIFY] `Engine/Core/DataModel/Part.h`
- `Engine::Signal Touched;` isminde bir event tanımı eklenecek.
- Sinyal, ClassBuilder ile Reflection sistemine kaydedilecek (`EngineCore.cpp` veya ilgili yerde).

---

### Fizik Sistemi (Physics)

#### [MODIFY] `Engine/Physics/PhysicsWorld.cpp`
- `PhysicsWorld::step()` metodunun içerisine, her fizik karesi hesaplandıktan sonra çalışacak şekilde `PendingContactEvents::instance().drainAll()` çağrısı eklenecek.
- Dönen `ContactEvent` listesindeki `id1` ve `id2` (Jolt Body UserData'ları) okunarak, bunlara karşılık gelen `Part` nesneleri `DataModel` üzerinden bulunacak.
- `part1->Touched.fire({ part2 })` ve `part2->Touched.fire({ part1 })` şeklinde sinyaller tetiklenecek.

---

### Luau Scripting (LuauRuntime)

#### [MODIFY] `Engine/Scripting/LuauRuntime/InstanceBinding.cpp`
- `__index` metametodu güncellenerek, istenen alan bir `Signal` ise, geriye özel bir "Signal Objesi" (veya Lua metatablosuna sahip userdata) döndürülecek.
- Bu Signal Objesi üzerinde `Connect` adında bir Lua C Function tanımlanacak.
- `Connect` fonksiyonu çalıştırıldığında, gönderilen Lua fonksiyonu bellekte tutulacak (Lua thread / registry referansı) ve C++ `Signal::connect` tarafına lambda olarak bağlanacak. Lambda tetiklendiğinde Luau VM'ine parametrelerle birlikte geri dönecek.

## Verification Plan

### Manual Verification
1. `NexusStudioEditor.exe` çalıştırılacak.
2. Bir `Script` bileşenine şu Lua kodu eklenecek:
   ```lua
   local part = script.Parent
   part.Touched:Connect(function(otherPart)
       print(part.Name .. " touched " .. otherPart.Name)
   end)
   ```
3. F5 tuşuna (Play) basıldığında, küpler zemine çarptığında terminalde çarpışma loglarının eksiksiz ve anlık olarak çıkıp çıkmadığı doğrulanacak.
