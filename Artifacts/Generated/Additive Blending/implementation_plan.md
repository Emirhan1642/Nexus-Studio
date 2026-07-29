# Additive Blending Implementation Plan (Katmanlı Animasyon - Faz 15)

Bu doküman, Nexus Studio'da Faz 15'in bir parçası olan "Additive Blending" (Eklemeli Animasyon) özelliğinin eksiksiz şekilde tamamlanması için yapılacak teknik değişiklikleri açıklamaktadır. Mevcut sistemde animasyonlar yalnızca doğrusal olarak harmanlanıyor (crossfade). Additive blending ile, bir animasyonun pozları referans pozdan çıkarılarak delta (fark) pozlar elde edilir ve bu deltalar mevcut taban animasyonların üzerine eklenir.

## User Review Required
> [!IMPORTANT]
> Bu değişiklik çekirdek matematik (Quaternion/Vector3) sınıflarını ve Animasyon Motoru temel döngüsünü (`AnimationPlayer::evaluate`) etkileyecektir. `AnimationTrack` sınıfına `IsAdditive` adında yeni bir property eklenecektir.

## Proposed Changes

### Core Math (Çekirdek Matematik)

Matematik kütüphanesine Additive blending işlemleri (rotasyon farkı bulma, bileşen bazlı çarpma/bölme) için eksik operatörler eklenecek.

#### [MODIFY] [Quaternion.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Quaternion.h)
- `Quaternion inverse() const;` metodu eklenecek (normalize edilmiş quaternion için eşlenik).
- `Quaternion operator*(const Quaternion& q) const;` metodu eklenecek (rotasyonları birleştirmek için).

#### [MODIFY] [Vector3.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/Math/Vector3.h)
- `Vector3 operator/(const Vector3& other) const;` eklenecek (scale farkı için).
- `Vector3 operator*(const Vector3& other) const;` eklenecek (scale birleşimi için).

---

### Animation Core (Animasyon Çekirdeği)

Kliplerin ve çalma izlerinin "additive" olup olmadığını anlaması sağlanacak.

#### [MODIFY] [AnimationClip.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationClip.h)
- `bool isAdditive = false;` eklenecek.

#### [MODIFY] [AnimationPlayer.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationPlayer.h)
- `PlayingTrack` yapısına `bool isAdditive = false;` eklenecek.
- `void play(...)` metoduna parametre eklenecek: `bool isAdditive = false`.
- `Math::Matrix4 blendAdditiveTransforms(const Math::Matrix4& basePose, const Math::Matrix4& additivePose, const Math::Matrix4& refPose, float weight) const;` yardımcı metodu eklenecek.

#### [MODIFY] [AnimationPlayer.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Animation/AnimationPlayer.cpp)
- `blendAdditiveTransforms` metodu uygulanacak:
  - `deltaPos = additivePos - refPos`
  - `deltaRot = refRot.inverse() * additiveRot`
  - `deltaScale = additiveScale / refScale`
  - `finalPos = basePos + (deltaPos * weight)`
  - `finalRot = baseRot * identity().slerp(deltaRot, weight)`
  - `finalScale = baseScale * (1.0 + (deltaScale - 1.0) * weight)`
- `evaluate` metodu güncellenerek `track.isAdditive` kontrolü yapılacak. Eğer additive ise `blendTransforms` yerine `blendAdditiveTransforms` çağrılacak. Referans poz olarak `skeleton.bones[i].bindPoseLocalTransform` kullanılacak.

---

### DataModel & Scripting API

Kullanıcıların veya betiklerin bir animasyon izini eklemeli olarak işaretleyebilmesi sağlanacak.

#### [MODIFY] [AnimationTrack.h](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/AnimationTrack.h)
- `bool isAdditive = false;` eklenecek.

#### [MODIFY] [AnimationTrack.cpp](file:///c:/Users/Emirhan/Desktop/Emirhan/Projects/Nexus%20Studio/Engine/Core/DataModel/AnimationTrack.cpp)
- `play()` metodu içinde `AnimationPlayer::play` çağrısına `isAdditive` veya `clip->isAdditive` (hangisi true ise) parametresi eklenecek.
- `Reflection::ClassBuilder` kısmına `IsAdditive` propertysi eklenecek.

## Verification Plan

### Automated Tests
- Mevcut ise animasyon testleri veya CMake derleme adımları koşturularak C++ sözdizimi hataları bulunmadığından emin olunacak.

### Manual Verification
- Değişiklik sonrası projeyi derleyip, editörün hata vermeden çalıştığını ve DataModel/AnimationTrack API'sinin yansımasını doğru şekilde gerçekleştirdiğini göreceğiz.
