# Katmanlı Animasyon (Layered Animation)

Şu anki `AnimationPlayer` sınıfı tek seferde sadece tek bir animasyonu (clip) oynatabiliyor. Karakterin yürürken aynı anda ateş etmesi, zıplarken veya başka bir eylem yaparken gövdesinin üst/alt kısımlarının farklı animasyonlardan etkilenmesi için **Katmanlı Animasyon (Layered Animation)** sistemine ihtiyacımız var. 

Bu aşamada animasyon oynatıcıyı (AnimationPlayer), aynı anda birden fazla parçayı (Track) öncelik (Priority) ve kemik maskeleme (Bone Masking) gibi kurallara göre harmanlayacak şekilde güncelleyeceğiz.

## User Review Required

> [!WARNING]
> Katmanlı animasyonlarda ağırlık (weight) ve öncelik (priority) gibi değerler `AnimationTrack` DataModel nesnesi üzerinden kontrol edilecek. Ekleme tipi animasyonlar (Additive) MVP (Minimum Viable Product) aşamasında karmaşıklığı artırabileceği için şimdilik atlanacak ve tamamen **Interpolated Override (Blend)** üzerine bir harmanlama sistemi kurulacak. Bu yaklaşım sizin için uygun mu?

## Proposed Changes

### EngineAnimation

#### [MODIFY] `Engine/Animation/AnimationPlayer.h`
- `currentClip` ve `activeTransition` yerine, oynatılan tüm animasyonları barındıran bir `struct PlayingTrack` eklenecek.
- `PlayingTrack` şunları içerecek:
  - `AnimationClip* clip`
  - `float time`
  - `float weight` ve `float targetWeight` (Fade in/out için)
  - `int priority`
  - `std::vector<int> boneMask` (Maskelenen kemikler; eğer boşsa tüm iskeleti etkiler)
- Yeni oynatma (play) metodu, mevcut animasyonların listesine yeni bir `PlayingTrack` ekleyecek.

#### [MODIFY] `Engine/Animation/AnimationPlayer.cpp`
- `evaluate` fonksiyonu tamamen yeniden yazılacak:
  1. Base pose (Skeleton Bind Pose) tüm kemikler için başlatılacak.
  2. Tüm aktif (PlayingTrack) animasyonlar *Priority (Öncelik)* sırasına göre sıralanacak (En düşükten en yükseğe).
  3. Her bir track için kemik pozları hesaplanıp, `boneMask` içerisinde ise ve track'in `weight` değeri sıfırdan büyükse ana poz üzerine interpolate (blend) edilecek.
  4. Fade out işlemi tamamlanan track'ler listeden çıkartılacak.

### EngineCore

#### [MODIFY] `Engine/Core/DataModel/AnimationTrack.h`
#### [MODIFY] `Engine/Core/DataModel/AnimationTrack.cpp`
- Lua/C++ tarafından erişilebilir yeni property'ler eklenecek:
  - `Priority` (Core, Idle, Movement, Action vb.)
  - `Weight` (1.0 tam etki)
  - `BoneMask` (Sadece belirli kemikleri etkilemesi için isim/index listesi)
- `play()` metodu, `AnimationPlayer`'ın yeni track-based listesine kendini kaydedecek şekilde uyarlanacak.

## Verification Plan

### Manual Verification
1. Editör üzerinden karakter (Humanoid) nesnesine iki farklı `AnimationTrack` eklenecek (Örn: Idle ve Attack).
2. "Attack" animasyonuna sadece belden yukarıdaki kemiklerin isimlerini içeren bir `BoneMask` uygulanacak ve Priority'si daha yüksek (Action) ayarlanacak.
3. Her iki animasyon aynı anda oynatıldığında, karakterin belden aşağısının (bacakların) "Idle" pozunda kalması, kollarının ise "Attack" animasyonunu gerçekleştirmesi gözlemlenecek.
4. Fade (Weight) değerleri ile yumuşak geçişin (Blending) doğru çalıştığı teyit edilecek.
