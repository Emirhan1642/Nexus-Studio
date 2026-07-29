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
