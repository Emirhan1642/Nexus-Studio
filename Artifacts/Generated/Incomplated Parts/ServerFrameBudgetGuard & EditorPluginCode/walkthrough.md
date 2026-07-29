# Script Timeout & Sonsuz Döngü Koruması (Walkthrough)

Doküman 10'da planlanan ancak koda dökülmemiş olan **Frame Bütçesi (Frame BudgetGuard)** ve **Editör Eklentisi Uyarı (Editor Plugin Warning)** sistemlerini başarıyla tamamladık.

## Ne Değişti?

### 1. `ServerFrameBudgetGuard` ile Motor Çökmelerine Karşı Koruma
Birden fazla script aynı kare (frame) içinde ağır işlemler yaptığında, hepsine tanınan "8ms" sınır yüzünden *toplamda* motorun 1 saniye donma riski vardı.
- `ScriptScheduler.h` dosyasına yeni bir `ServerFrameBudgetGuard` sınıfı eklendi.
- `ScriptScheduler.cpp` içindeki `update()` döngüsüne entegre edildi. Artık bir karenin içinde scriptlerin toplam çalışma süresi **10ms** sınırını aşarsa, kuyrukta sırasını bekleyen diğer script'ler *o frame içinde çalıştırılmaktan vazgeçilip bir sonraki frame'e erteleniyor.*
- Bu sayede 1000 tane script de olsa oyun asla kilitlenmeyecek, sadece script FPS'i düşecektir (Frame rate dropping for scripts, retaining render/physics frame rate).

### 2. `EditorPluginCode` İçin Zeki Zaman Aşımı Uyarı Sistemi
Oyun anında koşan scriptler saniyenin kesirlerinde hata fırlatmalıdır, ancak Editör içinde çalışan *Plugin* kodları (örneğin bütün haritaya rastgele çim yerleştiren bir plugin aracı) doğası gereği 2-3 saniye sürebilir.
- `ScriptWatchdog.cpp` güncellenerek `ScriptExecutionPhase::EditorPluginCode` için özel bir yol açıldı.
- Editör kodu **3 saniyeyi** geçtiğinde scripti vahşice kapatmak yerine bir "Durdurulsun mu?" simülasyonu çalışıyor (şimdilik konsolda C++ Error çıktısı olarak gösteriliyor, ilerde doğrudan Editör UI'ı üzerindeki bir pencereye bağlanabilir).
- Eğer kod 5 saniyeyi aşarsa (veya kullanıcı kapatmak isterse) script ancak o zaman güvenli şekilde kapatılıyor.

> [!TIP]
> **Nasıl Test Edilir?**
> Luau kodunuza kasten `while true do end` koyduğunuzda artık `std::cerr` (konsol) penceresinde "Luau Resume Error: Script execution budget exceeded" mesajını göreceksiniz ve motor çökmeyecek!

## Sonraki Adım

Faz 3-6 arasında sarkan tüm güvenlik önlemleri tamamlanmış oldu. Artık projenin asıl büyük yapı taşlarına dönmek için önümüzde harika iki seçenek var:

- **[Doküman 12] Karakter Kontrolcüsü (Humanoid):** Gerçek oynanabilirlik! Jolt fizik motoru üzerinden CharacterVirtual sınıfını entegre edip ekranda yürüyen, zıplayan, fizikle etkileşime giren ilk karakterimizi oluşturabiliriz.
- **[Doküman 8] C# Desteği:** Luau haricinde CoreCLR (C#) kullanarak çift dilli (dual-scripting) bir motor altyapısını atabiliriz.

Hangisiyle devam etmek istersin?
