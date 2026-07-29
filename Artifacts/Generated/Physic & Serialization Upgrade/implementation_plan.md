# Fizik ve Serileştirme Geliştirmeleri Planı

Bu plan, `Master_Index.md`'de belirtilen ve MVP seviyesinde bırakılan Fizik (Constraint ve Body Resizing) ve Serileştirme (ObjectRef ve Array) eksiklerini tamamlamayı hedefler.

## User Review Required
> [!IMPORTANT]
> - ObjectRef serileştirmesinde, referans verilen objeyi benzersiz bir yolla (path) bulmak için "İsim Tabanlı Hiyerarşi Yolu" (Örn: `Root/Child1/PartA`) yöntemi kullanılacaktır.
> - `Part` boyutlandırıldığında Jolt fizik motorundaki çarpışma kutusunun (BoxShape) güncellenmesi, varolan gövdenin şeklinin değiştirilmesiyle (SetShape) yapılacaktır.

## Proposed Changes

### 1. DataModel Serileştirmesinin Geliştirilmesi
#### [MODIFY] [DataModelSerializer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/DataModelSerializer.cpp)
- **Array Desteği:** `Kind::Array` tipindeki property'ler için, `arraySize` ve `arrayGet` metodları ile iterasyon yapılıp JSON array'ine dönüştürülmesi eklenecek. Deserialization kısmında ise `arraySet` kullanılarak veriler geri yüklenecek.
- **ObjectRef Desteği:** `Kind::ObjectRef` tipindeki property'ler için, hedeflenen objenin hiyerarşik adı hesaplanıp kaydedilecek. Geri yüklenirken (Deserialize), tüm DataModel yüklendikten sonra bu yolları (path) çözmek için bir ikinci geçiş (second pass / post-load) uygulanacak. (Bunun için `DataModelSerializer`'a bir `resolveReferences` adımı eklenecek).

#### [MODIFY] [DataModelSerializer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/DataModelSerializer.h)
- İkinci geçişte referansları çözmek için geçici bir takip (tracking) mekanizması eklenecek.

### 2. Fizik Constraint Limitleri (Hinge ve Spring)
#### [MODIFY] [HingeConstraint.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/HingeConstraint.h)
#### [MODIFY] [HingeConstraint.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/HingeConstraint.cpp)
- `Pivot` (Vector3) ve `Axis` (Vector3) property'leri eklenecek.
- Açı limitleri (LimitsEnabled, LowerLimit, UpperLimit) eklenip, Jolt tarafındaki `mLimitsMin` ve `mLimitsMax` ayarlarına bağlanacak.
- Reflection API (ClassBuilder) kaydı güncellenecek.

#### [MODIFY] [SpringConstraint.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/SpringConstraint.h)
#### [MODIFY] [SpringConstraint.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/SpringConstraint.cpp)
- `MinLength` ve `MaxLength` (float) property'leri eklenecek.
- Jolt `mMinDistance` ve `mMaxDistance` değerlerine bu yeni property'ler bağlanacak.
- Reflection API kaydı güncellenecek.

### 3. Dinamik Fizik Gövde Boyutlandırma (Dynamic Resizing)
#### [MODIFY] [Part.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.cpp)
- `setSize` metodunda fiziksel gövdenin (bodyId) şeklinin güncellenmesi sağlanacak. Jolt Physics'in `SetShape` fonksiyonu kullanılarak yeni `BoxShape` gövdeye atanacak ve fizik motoru uyarılacak (Activate).

## Verification Plan
1. Objeler yaratılıp `ObjectRef` değerleri atandıktan sonra serileştirilecek ve dosya kontrol edilecek.
2. `Part` nesnesinin boyutu kod üzerinden değiştirildiğinde (Size) fiziki çarpışmaların yeni boyuta göre çalışıp çalışmadığı manuel gözlemlenebilecek altyapı hazırlanmış olacak.
3. Hinge ve Spring constraint'lere atanan limitlerin motora yansıdığı gözden geçirilecek.
4. Walkthrough güncellenecek.
