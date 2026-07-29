# Script Timeout ve Sonsuz Döngü Korumasını Tamamlama

Önceki raporumda dosyalardaki arama (regex) kısıtlamasından dolayı altyapının hiç kurulmadığını belirtmiştim. Ancak kodları detaylı incelediğimde **LuauVM üzerinde ScriptWatchdog iskeletinin daha önceden kurulduğunu**, hatta `while true do end` gibi döngülerin 8ms'de durdurulduğunu fark ettim. 

Ancak Doküman 10'da belirtilen bazı **kilit özellikler koda dökülmemiş (yarım bırakılmış).** Bu planda o eksiklikleri tamamlayacağız.

## User Review Required
Bu geliştirmeler her karede (frame) çalışan ScriptScheduler'ı doğrudan etkileyecektir. Ayrıca sunucu tarafı frame bütçelemesi ekleneceği için script'lerin çok yoğun olduğu durumlarda FPS düşmesi yerine script'lerin çalışması bir sonraki kareye ertelenecektir. Lütfen planı onaylayın.

## Open Questions
- Şu an editörde tam fonksiyonel bir UI log/output paneli bulunmuyor. Bu yüzden hata loglarını şimdilik C++ standart hata çıktısına (`std::cerr`) ve "Are you sure?" diyaloğunu da doğrudan editör sistemini beklemeden durduracak şekilde (veya geçici mock fonksiyonla) ekliyorum. Uygun mudur?

## Proposed Changes

### Scripting Engine (LuauRuntime)

#### [MODIFY] [ScriptScheduler.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.h)
- `ServerFrameBudgetGuard` sınıfı eklenecek. Bu sınıf, aynı kare içinde çalışan *tüm scriptlerin* toplam çalışma süresini ölçecek.

#### [MODIFY] [ScriptScheduler.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptScheduler.cpp)
- `ScriptScheduler::update(float deltaTime)` döngüsüne `ServerFrameBudgetGuard` entegre edilecek.
- Eğer bir karedeki toplam script çalışma süresi 10ms'yi geçerse (`MAX_TOTAL_SCRIPT_TIME_PER_FRAME`), kuyrukta kalan diğer script'ler *o karede çalıştırılmayıp bir sonraki kareye* ertelenecek. Bu sayede motorun donması engellenecek.

#### [MODIFY] [ScriptWatchdog.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Scripting/LuauRuntime/ScriptWatchdog.cpp)
- `onInterrupt` callback'ine Doküman 10, Bölüm E'de bahsedilen `EditorPluginCode` için özel uyarı mantığı eklenecek. Zaman sınırı dolduğunda (örn. 3 saniye) hemen öldürmek yerine kullanıcıya "Durdurulsun mu?" uyarısı verilecek (şimdilik konsol mesajı/flag ile simüle edilecek).

## Verification Plan

### Automated/Manual Tests
- Oyunda 2 adet Script oluşturulup ikisine de ağır bir işlem veya sonsuz döngü verilecek.
- Tek bir script'in zaman aşımında (8ms) motoru dondurmadan kendi kendine hata fırlatıp kapandığı gözlemlenecek.
- Çok sayıda script olduğunda (Frame Budget aşıldığında) FPS'in dibe vurmadığı, script'lerin iş yükünün bir sonraki frame'lere dağıtıldığı test edilecek.
