# Katmanlı Animasyon (Layered Animation)

Oyun motorunun animasyon oynatıcısı (`AnimationPlayer`), karakterin aynı anda birden fazla animasyonu (örneğin yürürken ateş etmesi) harmanlayabilmesi için **Katmanlı Animasyon (Layered Animation)** sistemiyle güncellendi.

## Neler Yapıldı?

1. **Çoklu Animasyon Desteği (`PlayingTrack`)**
   - Eski sistemdeki tekil `currentClip` mantığı tamamen kaldırılarak yerine `std::vector<PlayingTrack>` dizisi getirildi. 
   - Artık aynı anda istenilen sayıda animasyon, kendi zamanlayıcısı (time), etki gücü (weight) ve önceliği (priority) ile bağımsız olarak oynatılabiliyor.

2. **Öncelik Sistemi (Priority)**
   - Animasyonlar `evaluate` (kemik pozisyonlarını hesaplama) aşamasında öncelik (Priority) sırasına göre diziliyor. 
   - Örneğin; "Core" (Duruş) animasyonunun üzerine "Movement" (Yürüme) biniyor, onun da üzerine "Action" (Ateş Etme) binebiliyor. Yüksek öncelikli animasyon, kendinden alt seviyedekilerin hareketlerini eziyor (Override).

3. **Kemik Maskeleme (Bone Masking)**
   - Animasyonların sadece belirli kemiklerde çalışmasını sağlayan `BoneMask` eklendi.
   - Bir animasyona maske uygulandığında (Örn: Sadece Sağ Kol), o animasyon tüm iskeleti değil, sadece ilgili kemikleri etkiliyor. Geri kalan kısımlarda bir alt önceliğe sahip olan (Örn: Yürüme) animasyon çalışmaya devam ediyor.

4. **Yumuşak Geçiş (Fade In / Fade Out)**
   - `targetWeight` ve `weightSpeed` parametreleri sayesinde, animasyonlar aniden başlamak veya bitmek yerine yavaş yavaş etkisini artırıp (Fade In) azaltabiliyor (Fade Out).

5. **`AnimationTrack` Güncellemesi**
   - `Engine/Core/DataModel/AnimationTrack` nesnesi artık `Priority`, `Weight` ve `BoneMask` property'lerine sahip. 
   - Bu özellikler `ClassBuilder` aracılığıyla Reflection (C++ -> Editor/Lua) sistemine de kaydedildi.

## Test Adımları

- Editörü başlatın.
- Karakter modelinize (Humanoid barındıran) iki adet `AnimationTrack` nesnesi ekleyin.
- İlkine yürüme veya koşma animasyonu atayın (Priority: 1000).
- İkincisine örneğin kılıç savurma veya ateş etme animasyonu atayın (Priority: 1003 - Action). İkinci track'in `BoneMask` listesine sadece belden yukarıdaki (Spine, Arm_R, Arm_L vb.) kemiklerin indekslerini girin.
- İki animasyonu da Play üzerinden çalıştırdığınızda karakterinizin bir yandan koşarken, üst bedeniyle eylem yaptığını sorunsuzca gözlemleyebileceksiniz.

> [!TIP]
> İleride eklenecek olan Lua Scripting (Aşama 6) entegrasyonu sayesinde bu animasyon katmanlarını oyun kodunun içerisinden `Track->Play()` veya `Track->Stop()` çağrılarıyla dinamik olarak yönetebileceksiniz.
