# Task: Katmanlı Animasyon (Layered Animation)

- `[x]` **Görev 1: AnimationPlayer Başlık Dosyası Güncellemesi**
  - `[x]` `Engine/Animation/AnimationPlayer.h` dosyasında `PlayingTrack` struct'ı tanımlanacak.
  - `[x]` `currentClip` yerine `std::vector<PlayingTrack> activeTracks` yapısı eklenecek.
  - `[x]` `play` ve `stop` fonksiyonları `AnimationClip` ile birlikte weight ve priority alacak şekilde ayarlanacak.

- `[x]` **Görev 2: AnimationPlayer Kaynak Dosyası Güncellemesi**
  - `[x]` `Engine/Animation/AnimationPlayer.cpp` içindeki `evaluate` metodu baştan yazılacak.
  - `[x]` Aktif track'ler Priority sırasına göre sıralanacak.
  - `[x]` Her bir track'in pozisyonları hesaplanıp, eğer `boneMask` içerisinde ise ana poza eklenecek (Blend).
  - `[x]` Fade-in / Fade-out mekanizması `weight` ve `targetWeight` ile işlenecek.

- `[x]` **Görev 3: AnimationTrack Nesnesi Güncellemesi**
  - `[x]` `Engine/Core/DataModel/AnimationTrack.h` ve `.cpp` dosyalarına `Priority`, `Weight`, ve `BoneMask` eklenecek.
  - `[x]` `ClassBuilder` üzerinden özellikleri Reflection sistemine dahil edilecek.
  - `[x]` `play()` ve `stop()` komutları `AnimationPlayer` ile düzgün haberleşecek.

- `[x]` **Görev 4: Derleme ve Test**
  - `[x]` CMake üzerinden proje derlenecek.
  - `[x]` Maskelenmiş (sadece kollar) bir eylem animasyonu ile alt bedeni hareket ettiren bir yürüme animasyonunun sorunsuz harmanlandığı kontrol edilecek.
