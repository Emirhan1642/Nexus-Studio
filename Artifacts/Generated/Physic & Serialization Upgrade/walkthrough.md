# Karakter Kontrolcüsü Geliştirmeleri

Karakter kontrolcüsünün (Humanoid) daha stabil çalışabilmesi ve animasyon sistemleriyle bağ kurabilmesi için bir dizi altyapı iyileştirmesi yapıldı.

## Neler Yapıldı?

### 1. Matematik Altyapısı Güçlendirildi
Ters kinematik hesaplamalarını yapabilmek için eksik olan bazı 3D matematik fonksiyonları eklendi:
- [Vector3.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Vector3.h) içerisine **Cross Product** (Çapraz Çarpım) ve **Dot Product** (Nokta Çarpım) fonksiyonları eklendi.
- [Quaternion.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Quaternion.cpp) içerisine, bir eksen ve açı vererek rotasyon üretmeyi sağlayan `fromAxisAngle` statik metodu eklendi.

### 2. Ters Kinematik (Two-Bone IK) Algoritması Yazıldı
- [TwoBoneIK.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/IK/TwoBoneIK.h) dosyasında sadece yer tutucu olarak duran fonksiyon; **Kosinüs Teoremi (Law of Cosines)** kullanarak bacak ve diz (veya kol/dirsek) kırılma açılarını hesaplayan gerçek bir solver'a dönüştürüldü.
- `poleVector` (diz yönü) ile oluşturulan hedef düzlem (bending plane) baz alınarak üst (kalça) ve alt (diz) kemik rotasyonları hesaplandı.
- `Humanoid.cpp` içerisinde IK'nin bu rotasyonları alıp doğrudan matrislere (finalBoneTransforms) uygulaması için kanca (hook) kodları temizlendi ve yorum satırlarıyla entegrasyon formülü gösterildi.

### 3. Ragdoll Durumundan Çıkış (Kalkma)
- [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.h) dosyasına `exitRagdoll()` fonksiyonu eklendi.
- Bu fonksiyon çağrıldığında:
  - Ragdoll için yaratılan tüm eklemler (Constraints) `PhysicsWorld` üzerinden temizleniyor.
  - Fiziksel gövdeler (Torso, Head, vb.) Jolt Physics'ten tamamen siliniyor.
  - Karakter kapsülü (CharacterVirtual) yeniden inşa ediliyor ve karakter tekrar ayağa kalkıp kontrol edilebilir duruma (`Idle`) geçiyor.
- Lua tabanlı betiklerin (Script) bu özelliği tetikleyebilmesi için `Reflection API` aracılığıyla "ExitRagdoll" metodu motora kaydedildi.

> [!TIP]
> Artık Lua kodunuzda `humanoid:EnterRagdoll()` dedikten sonra istediğiniz bir gecikme (wait) ile `humanoid:ExitRagdoll()` çağırarak karakteri tekrar ayağa kaldırabilirsiniz.

## 4. Fizik Constraint ve Şekil Geliştirmeleri (Physics Enhancements)
- **HingeConstraint:** [HingeConstraint.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/HingeConstraint.h) üzerine `Pivot` ve `Axis` property'leri eklendi. Menteşenin nerede ve hangi eksende döneceği artık serbestçe belirlenebilir. Aynı zamanda açı limitlerini kısıtlamak için `LimitsEnabled`, `LowerLimit` ve `UpperLimit` özellikleri Jolt physics'e bağlandı.
- **SpringConstraint:** [SpringConstraint.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/SpringConstraint.h) üzerine yayın ne kadar esneyebileceğini limitleyen `LimitsEnabled`, `MinLength`, `MaxLength` özellikleri eklendi.
- **Dinamik Fiziksel Boyutlandırma:** `Part` objesinin `Size` özelliği koddan değiştiğinde, fizik motorundaki çarpışma kutusu eski boyutta kalıyordu. [Part.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Part.cpp) içerisindeki `setSize` metodu Jolt Physics'in `SetShape` API'sini kullanacak şekilde güncellendi. Artık objelerin boyutları gerçek zamanlı değişirken fizikleri de anında uyum sağlıyor.

## 5. Serileştirme (Serialization) Derinleştirildi
- [DataModelSerializer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/DataModelSerializer.cpp) yeniden yazılarak eksik olan karmaşık veri tiplerinin (Array, ObjectRef) serileştirilmesi eklendi.
- **Array Desteği:** Reflection üzerinde `Kind::Array` tipindeki veriler otomatik iterasyonla JSON'a aktarılabiliyor ve yüklenebiliyor.
- **ObjectRef Desteği (Referanslar):** Bir obje bir başka objeyi referans aldığında (`Kind::ObjectRef`), obje bellekteki adresi yerine hiyerarşi yoluyla (Örn: `Root/Child1/PartA`) JSON'a kaydediliyor. Yükleme esnasında (Deserialize) özel bir "Resolve Pass" (Çözümleme adımı) uygulanarak yol bulunup objeye atanıyor. Böylece Constraint'lerin bağlandığı `Part0` ve `Part1` verileri save/load sonrası kırılmıyor.
