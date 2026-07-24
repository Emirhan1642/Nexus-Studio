# Play/Stop Durum Serileştirmesi (Serialization)

Editörde sahneyi test ederken ("Play" modunda) obje eklenmesi veya özelliklerin değişmesi durumunda, "Stop" butonuna basıldığında sahnenin orijinal haline geri dönmesini sağlayan **DataModel Serialization** altyapısı tamamlandı.

## Neler Yapıldı?

1. **`DataModelSerializer` Sınıfı**
   - Hali hazırda kullanmakta olduğumuz C++ Reflection (`TypeRegistry`) sistemi baz alınarak dinamik bir JSON serileştirme sistemi yazıldı.
   - `nlohmann_json` kütüphanesi projeye başarıyla dahil edildi.
   - `DataModelSerializer::serialize(Instance)` metodu sayesinde, herhangi bir nesne ve alt çocukları (recursive olarak) içerdikleri tüm primitif (float, int, string, Vector3 vb.) özelliklerle birlikte JSON formatına çevrilebiliyor.
   - `DataModelSerializer::deserialize(JSON)` metodu, kaydedilen o JSON'ı okuyup Reflection factory metodlarını (`createInstance`) tetikleyerek objeleri sıfırdan ve eksiksiz bir şekilde hafızada tekrar canlandırabiliyor.

2. **`Workspace` Odaklı Yedekleme**
   - Play tuşuna (F5) basıldığında, tüm DataModel yerine sadece sahne nesnelerinin barındığı `Workspace` JSON formatında yedekleniyor.
   - Oyun çalışırken (Fizik motoru işlerken, karakter gezinirken) veya editör üzerinden objelerin konumu, rengi vb. değiştiğinde sahne bu değişimlerden etkileniyor.
   - Stop (F5) tuşuna basıldığında ise kirlenmiş (dirty) `Workspace` nesnesi tamamen yok ediliyor (destroy). Ardından baştaki JSON yedeğinden temiz bir `Workspace` baştan yaratılarak `DataModel`'e geri ekleniyor. Böylece nesneler, scriptler, fizik durumları %100 orijinal konumlarına dönüyor.

3. **Eski Kodların Temizliği**
   - Daha öncesinde sadece `Part` tipine özel yazılmış olan statik ve eksik `DataModelSnapshot` yapısı kaldırıldı. 

## Test Adımları

- Editörü başlatın.
- Sahneye bir obje (Part vb.) ekleyin veya mevcut objelerin pozisyon, renk (Albedo) veya fizik özelliklerini değiştirin.
- **Play (F5)** butonuna basın. (Eğer objenin altından zemini silerseniz veya ona bir kuvvet uygularsanız düşecek, konumu değişecektir.)
- Animasyon ve fizik simülasyonu belli bir süre oynadıktan sonra tekrar **Stop (F5)** butonuna basın.
- Sahnedeki objelerin tam olarak oyunu başlattığınız o ilk ana geri döndüğünü (renk, konum, boyut) göreceksiniz. Yeni eklenen objeler de eğer Play modunda eklendiyse Stop olunca yok olacak, Play öncesinde eklendiyse geri gelecektir.

> [!TIP]
> Bu altyapı ileride "Projeyi Kaydet (Save/Load)" özelliği ve ağ üzerinde (Network) tam `DataModel` replikasyonu yapabilmek için çok güçlü bir iskelet sunmaktadır. Artık objelerin özelliklerini tek tek koda hardcode etmeden dinamik olarak serileştirebiliyoruz!
