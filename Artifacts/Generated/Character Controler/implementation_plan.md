# Karakter Kontrolcüsü (Humanoid) Eksiklerinin Tamamlanması

Bu plan, `Humanoid` sınıfındaki iki büyük eksikliği (MVP olarak bırakılmış özellikleri) tamamlamayı hedefler: Ragdoll durumundan çıkış ve tam çalışan bir Two-Bone IK (Ters Kinematik) altyapısı.

## User Review Required
> [!IMPORTANT]
> Bu aşamada temel matematik kütüphanesine (`Vector3`, `Quaternion`) bazı eklemeler yapacağız. Matematik sınıfları motorun birçok yerinde kullanıldığı için dikkatli test edilmesi gerekecektir.

## Proposed Changes

### 1. Math Sınıflarının Genişletilmesi
İki kemikli ters kinematik (Two-Bone IK) hesaplamaları yapabilmek için vektör çarpımı (Cross Product) ve eksen-açı (Axis-Angle) tabanlı Quaternion oluşturma fonksiyonlarına ihtiyacımız var.
#### [MODIFY] [Vector3.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Vector3.h)
- `dot(const Vector3&)` (Nokta Çarpım) eklenecek.
- `cross(const Vector3&)` (Çapraz Çarpım) eklenecek.
- `normalize()` (Vektör normalizasyonu) eklenecek.
#### [MODIFY] [Quaternion.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Quaternion.h)
#### [MODIFY] [Quaternion.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Quaternion.cpp)
- `fromAxisAngle(const Vector3& axis, float angle)` eklenecek.

### 2. Two-Bone IK Algoritmasının Yazılması
#### [MODIFY] [TwoBoneIK.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/IK/TwoBoneIK.h)
- Şu an sadece `Identity` dönen boş (stub) fonksiyon, Law of Cosines (Kosinüs Teoremi) kullanılarak ve Pole Vector (Diz yönü) baz alınarak gerçek Quaternion rotasyonları üretecek şekilde güncellenecek.

### 3. Ragdoll Çıkış Mantığının (Exit Ragdoll) Eklenmesi
#### [MODIFY] [Humanoid.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.h)
#### [MODIFY] [Humanoid.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/Humanoid.cpp)
- `void exitRagdoll();` fonksiyonu eklenecek. Bu fonksiyon:
  - Üretilen Ragdoll uzuvlarını (Head, Torso, Arms) physics dünyasından silecek.
  - Orijinal `CharacterVirtual` kapsülünü yeniden yaratacak.
  - Karakteri tekrar `Idle` state'ine geçirecek.
- Reflection API (ClassBuilder) üzerinden Lua'nın kullanabilmesi için `ExitRagdoll` metodu kaydedilecek.

#### [MODIFY] [HumanoidStateMachine.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/HumanoidStateMachine.cpp)
- State makinesi, ragdoll state'inin dış müdahale ile değiştirilmesine izin verecek şekilde uyarlanacak.

## Verification Plan
1. `Vector3` ve `Quaternion` eklentilerinin syntax hataları vermediği doğrulanacak.
2. `Humanoid::exitRagdoll` çağrıldığında fizik motorunda memory-leak olmadan rigid body'lerin temizlendiği incelenecek.
3. Walkthrough dosyası güncellenecek.
