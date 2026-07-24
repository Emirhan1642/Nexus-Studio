# Play/Stop Durum Serileştirmesi (Serialization)

Şu anki editörümüzde "Play" tuşuna (F5) basıldığında sahne kaydedilmiyor ve "Stop" tuşuna basıldığında eski haline geri dönmüyor. Bu aşamada, mevcut DataModel ağacını (hali hazırda kurulu olan `TypeRegistry` Reflection sistemini kullanarak) JSON formatına çevirip kaydetmeyi ve oyun durduğunda (Stop) o JSON'dan sahneyi tekrar yaratmayı (Deserialize) planlıyorum. 

Aynı zamanda bu sayede ileride **Save/Load (.nexus) projeleri oluşturmanın da temelini** atmış olacağız!

## Kullanılacak Teknolojiler
- C++ Reflection sistemi (`TypeRegistry`)
- JSON Kütüphanesi (`nlohmann_json`) (Projede `ThirdParty` klasörü altında bulunuyor)

## User Review Required

> [!WARNING]
> Bu aşamada nesneler arası referansları (ObjectRef) MVP (Minimum Viable Product) aşamasında olduğumuz için şimdilik atlamayı planlıyorum. Çünkü ObjectRef'lerin doğru şekilde Serialization / Deserialization işlemleri (Guid tabanlı) daha detaylı bir mimari (Guid map / Reference Fixup) gerektirir. Sadece primitif veri türlerini (String, Float, Vector3, Enum) ve Hiyerarşik Ebeveyn-Çocuk ilişkilerini kaydedip geri yükleyeceğiz. Bu yaklaşım şu anki Part ve Humanoid gibi objeleri %100 kapsar. Bu yaklaşım sizin için uygun mu?

## Proposed Changes

### EngineCore

#### [NEW] `Engine/Core/DataModel/DataModelSerializer.h`
DataModel ağacındaki bir `Instance` referansını alıp onu `nlohmann::json` yapısına dönüştüren statik metodları barındıracak.
- `static nlohmann::json serialize(const std::shared_ptr<Instance>& root);`
- `static std::shared_ptr<Instance> deserialize(const nlohmann::json& j);`

#### [NEW] `Engine/Core/DataModel/DataModelSerializer.cpp`
`TypeRegistry` üzerinden `find(className)` yapılarak objenin tüm primitif özelliklerini ve çocuklarını yinelemeli (recursive) şekilde JSON'a yazan ve JSON'dan tekrar okuyup `createInstance` aracılığıyla sıfırdan oluşturan implementasyon.
- Float, Int, Bool, std::string
- Engine::Math::Vector3

#### [MODIFY] `Engine/Core/CMakeLists.txt`
Yeni yazdığımız `DataModelSerializer.cpp` kaynak dosyasına eklenecek ve nlohmann_json entegre edilecek.
- `target_link_libraries(EngineCore PUBLIC ... nlohmann_json::nlohmann_json)`

### Editor

#### [MODIFY] `Editor/Main.cpp`
Şu an hardcoded olarak sadece Part'ların pozisyon ve renklerini kaydeden eski `DataModelSnapshot` sınıfı silinecek ve yerine geliştirdiğimiz Reflection tabanlı `DataModelSerializer` kullanılacak.
- Play'e basıldığında `snapshotJson = DataModelSerializer::serialize(DataModel::instance());`
- Stop'a basıldığında `DataModel::instance()->clear();` (tüm objeler temizlenecek) ardından `DataModelSerializer::deserialize` kullanılarak DataModel yeniden yüklenecek.

## Verification Plan

### Manual Verification
1. Editörde rastgele bir obje yaratılacak.
2. Rengi ve pozisyonu değiştirilecek.
3. Play (F5) tuşuna basılıp objenin fizik motoru sayesinde düştüğü izlenecek.
4. Stop (F5) tuşuna tekrar basıldığında objenin eski pozisyonuna ve orijinal haline **sorunsuz ve tam olarak** geri döndüğü teyit edilecek.
