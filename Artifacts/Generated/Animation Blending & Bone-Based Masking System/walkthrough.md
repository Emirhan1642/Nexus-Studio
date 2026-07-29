# Karakter Kontrolcüsü Geliştirmeleri

Karakter kontrolcüsünün (Humanoid) daha stabil çalışabilmesi ve animasyon sistemleriyle bağ kurabilmesi için bir dizi altyapı iyileştirmesi yapıldı.

## Neler Yapıldı?

### 1. Jolt Fizik Çökmelerinin ve Bellek Sızıntılarının Giderilmesi
- Jolt Physical Engine test esnasında kısıtlama yaratılırken iki defa aynı nesneyi tekli-kilit (BodyLockWrite) ile kilitlemeye çalıştığı için `0xc0000005` ihlali veriyordu. Bu sorun, Jolt'un `BodyLockMultiWrite` çoklu kilit fonksiyonu kullanılarak giderildi.
- Test ortamı yok edildiğinde Jolt'un hala hayatta olan bedenlere (bodies) sahip olması nedeniyle kapanışta çökmesi engellendi ve test bitiminde Instance'ların doğru temizlenmesi sağlandı.

### 2. Serileştirmede Kalıtım Sorunu Çözüldü
- Base (temel) sınıflardan miras alınan Reflection özellikleri (örneğin `Instance` sınıfındaki "Name") DataModelSerializer içerisinde dikkate alınmıyordu, bu nedenle objeler her zaman varsayılan `"Instance"` isminde kalıyordu.
- `DataModelSerializer::serializeRecursive` ve `deserializeRecursive` fonksiyonlarına döngüsel kalıtım araması eklenerek bu sorun giderildi.
- `ObjectRefSerialization` testi de dahil olmak üzere 11 testin tümü başarıyla geçti.

### 3. Matematik Altyapısı Güçlendirildi
Ters kinematik hesaplamalarını yapabilmek için eksik olan bazı 3D matematik fonksiyonları eklendi:
- [Vector3.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Vector3.h) içerisine **Cross Product** (Çapraz Çarpım) ve **Dot Product** (Nokta Çarpım) fonksiyonları eklendi.
- [Quaternion.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Quaternion.cpp) içerisine, bir eksen ve açı vererek rotasyon üretmeyi sağlayan `fromAxisAngle` statik metodu eklendi.

### 4. Ters Kinematik (Two-Bone IK) Algoritması Yazıldı
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

## 6. Jolt CharacterVirtual Entegrasyonu (Humanoid)
- **Kapsül Çarpışması:** `Humanoid` sınıfı, `RootPart` özelliklerine bağlı olarak Jolt Physics'te bir `CharacterVirtual` (Kapsül) yaratacak şekilde kodlandı. Kapsül boyutlarının `Studs`'dan `Meters`'a dönüştürülmesi sağlanarak, zemine tam temas eden mükemmel bir Kapsül fiziği sağlandı.
- **RootPart ve Kinematik Vücut:** Sanal karakter oluşturulurken ana gövde olan `RootPart`, yerçekiminden iki kere etkilenip motorla çakışmaması için otomatik olarak `Kinematic` hıza geçiriliyor. Ayrıca `CharacterVirtual`, kendi bedeniyle çarpışmasını (IgnoreSingleBodyFilter) göz ardı edebiliyor.
- **PhysicsWorld Sinyalleri:** Fizik dünyasına, Jolt adımları öncesinde ve sonrasında çalışacak `prePhysicsUpdate` ve `postPhysicsUpdate` kancaları eklendi. `Humanoid`, Jolt döngüsüne saniyede 60 kez bağlanarak manuel yerçekimi, merdiven tırmanma (WalkStairsStepUp) ve zemine yapışma (StickToFloorStepDown) döngülerini işliyor.
- **Test ve Onay (HumanoidTests):** Karakterin havadan süzülüşünü, yerçekimi etkisini, zıplama kuvvetini ve `moveTo()` fonksiyonuyla yatay yürüyüş yapabilme kabiliyetini test eden Unit Test yazıldı ve tüm aşamalardan hatasız geçti!

## 7. Animasyon Geçişleri (Blending) ve Kemik Maskeleme
Karakterlerin oyun içindeki durumlarına (Idle, Walk, Jump vb.) göre pürüzsüz animasyon geçişleri (Crossfade Blending) ve bölgesel animasyon maskeleme sistemi DataModel ve State Machine'e entegre edildi.
- **İsim Bazlı Kemik Maskeleme:** Geliştiricilerin belirli animasyonları sadece istedikleri kemiklerde (Örn: Sadece kollar) oynatabilmeleri için `AnimationTrack::addBoneMask(std::string name)` ve `removeBoneMask` API'si eklendi. Maskeleme varsayılan olarak **Recursive (hiyerarşik)** çalışır; yani "RightUpperArm" maskelendiğinde "RightHand" de otomatik olarak maskelenir ve `AnimationPlayer` bu mantıkla çalışır.
- **State Machine Entegrasyonu:** `Humanoid` sınıfına `idleAnimation`, `walkAnimation`, `jumpAnimation` ve `fallAnimation` adında `AnimationClip` özellikleri eklendi. `HumanoidStateMachine` durumu her değiştiğinde (örneğin dururken yürümeye başladığında) arka planda otomatik olarak eski `AnimationTrack` 0.2 saniyelik solma (fade out) ile durdurulur ve yeni durumun animasyonu 0.2 saniyelik belirmeyle (fade in) başlatılır.
- **Dinamik Öncelik (Priority) ve Harmanlama:** `AnimationPlayer`, ağırlıkları (weights) ve öncelikleri hesaba katarak Quaternion Slerp ve Vector Lerp işlemleriyle animasyonları pürüzsüzce karıştırır (Crossfade).
- **Test (AnimationTests):** Blending matematiğini (%50 %50 harmanlama) ve kemik maskeleme altyapısını sınayan `TestAnimationBlending` ve `TestBoneMasking` unit testleri eklendi, başarıyla tamamlandı.

## 8. Asset Browser Alt-Varlık (Sub-Asset) Entegrasyonu
Skeletal Mesh (İskeletli Model) import süreçlerini desteklemek amacıyla, çoklu varlık içeren (Mesh + Animasyonlar) dosyalar için gelişmiş Asset Browser entegrasyonu tamamlandı.
- **Sanal Varlıklar (Virtual Assets):** İçinde animasyonlar barındıran `.fbx` dosyaları artık Asset Database tarafından özel olarak işlenir. FBX içindeki `Mesh`, `Skeleton` ve `AnimationClip` gibi alt bileşenler, diskte fazladan bir dosya yaratmadan **Sanal Varlık (Virtual Asset)** olarak özel GUID'ler ile hafızada kayıt altına alınır.
- **Dizin İçi Gezinme (FBX Klasörü):** Geliştiriciler Asset Browser üzerinde bir `.fbx` dosyasına **çift tıkladığında**, sistem FBX dosyasının içine girer (sanki bir klasörmüş gibi) ve içindeki animasyonları listeler.
- **Sürükle-Bırak Desteği:** FBX'in içinden seçilen bir `Idle` veya `Walk` animasyonu, tut-sürükle (Drag & Drop) yapılarak doğrudan Properties Panel'indeki `Humanoid` özelliklerine (Örn: `idleAnimation`) bırakılabilir. `PropertiesPanel` bu sanal GUID'yi otomatik olarak tanır ve Inspector'da gösterir.
- **Hafıza Optimizasyonu:** `AssetDatabase::getAnimationClip` yardımıyla bir animasyon istendiğinde, ana FBX dosyasının verisi (`ImportedSkeletalMesh`) kullanılarak animasyon klipsine referans (`std::shared_ptr` ile Aliasing Constructor kullanılarak) döndürülür, böylece bellek israfının önüne geçilir.
