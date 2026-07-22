# Master Index — Oyun Motoru Projesi
## Tüm Dokümanların Haritası, Bağımlılıkları ve Önerilen Geliştirme Sırası

Bu doküman, şu ana kadar üretilen 16 dokümanı tek bir referans noktasında topluyor. **Önemli bir uyarı:** Bu dokümanlar sohbet boyunca doğal merak sırasına göre yazıldı (Faz 6'dan sonra Faz 7, sonra Script Timeout, sonra Faz 8, sonra Karakter Kontrolcüsü gibi) — ama bu, **gerçek geliştirme sırası olmak zorunda değil**. Bu doküman, o dokümanları gerçek bağımlılıklarına göre yeniden sıralıyor.

---

## Bölüm A — Doküman Envanteri

| # | Doküman Adı | Ne İçeriyor | Ana Katkısı |
|---|---|---|---|
| 1 | `Oyun_Motoru_Implementation_Plan.md` | Genel vizyon, teknoloji yığını, 8 fazlı üst-seviye yol haritası | Projenin "anayasası" |
| 2 | `Faz0_Faz1_Teknik_Detay.md` | CMake iskeleti, temel Reflection sistemi, DataModel/Instance | Reflection'ın ilk hali |
| 3 | `Faz1_Derinlestirme.md` | Kalıtım zinciri (static init order fix), Enum/Array/ObjectRef property tipleri, metod reflection | Reflection'ın tamamlanmış hali |
| 4 | `Faz2_Render_Pipeline.md` | RenderProxy deseni, Clustered Forward, bgfx akışı, PBR, gölgeleme | DataModel'i ekrana bağlama |
| 5 | `Faz3_Luau_Scripting.md` | Luau VM, generic `__index`/`__newindex`, sandboxing, Signal sistemi, coroutine tabanlı `wait()` | Reflection'ı script'e bağlama |
| 6 | `Faz4_Editor.md` | ImGui, Viewport (render-to-texture), Explorer, Properties, Gizmo, Undo/Redo, Play/Stop | Reflection'ı editör UI'ına bağlama |
| 7 | `Faz5_Fizik_Jolt.md` | Jolt entegrasyonu, çift yönlü senkronizasyon, Touched event, Constraint'ler, stud/metre ölçeği | Fiziksel davranış |
| 8 | `Faz6_Networking.md` | GNS, DataModel replication, client-side prediction, RemoteEvent | Çoklu oyuncu temeli |
| 9 | `Faz7_Gelismis_Grafik.md` | VCT/SSGI, post-processing, Discrete LOD, node-based materyal editörü | Görsel kalite (sürekli iyileştirme) |
| 10 | `Script_Timeout_Sonsuz_Dongu_Korumasi.md` | Luau `interrupt` callback, bağlama göre bütçe, watchdog | Script güvenliği |
| 11 | `Faz8_CSharp_Destegi.md` | CoreCLR hosting, ikinci reflection köprüsü, güven modeli ayrışması | İkinci scripting dili |
| 12 | `Karakter_Kontrolcusu_Humanoid.md` | CharacterVirtual, state machine, prediction, ragdoll | Oynanabilir karakter |
| 13 | `Skeletal_Animation_Sistemi.md` | Bone/Skinning/GPU deformasyon, IK, FBX import, animasyon networking'i | Karakter görselleştirmesi |
| 14 | `Asset_Browser_Import_Akisi.md` | AssetDatabase/GUID, dosya izleme, thumbnail, hot-reload, sürükle-bırak | Editör ↔ dosya sistemi köprüsü |
| 15 | `Katmanli_Animasyon_Layered_Animation.md` | Bone mask, additive blending, çoklu katman kompozisyonu | Animasyonun esnekleşmesi |
| 16 | `Interest_Management_Derinlestirme.md` | Spatial grid, hysteresis, öncelik tabanlı bant genişliği, dormancy | Networking'in ölçeklenmesi |

---

## Bölüm B — Gerçek Bağımlılık Grafiği

Aşağıdaki diyagram, "hangi doküman hangisini önkoşul olarak gerektiriyor" sorusuna cevap veriyor. Ok yönü "bağımlıdır" anlamına gelir.

```
                                    [1] Implementation Plan (vizyon)
                                              │
                                              ▼
                                    [2] Faz 0/1 (Reflection temel)
                                              │
                                              ▼
                                    [3] Faz 1 Derinleştirme
                                    (kalıtım + karmaşık tipler)
                                              │
                    ┌─────────────────────────┼─────────────────────────┐
                    ▼                         ▼                         ▼
              [4] Faz 2 (Render)       [5] Faz 3 (Luau)          [11] Faz 8 (C#)
                    │                         │                         │
                    │                         ▼                         │
                    │                [10] Script Timeout                │
                    │                         │                         │
                    └────────────┬────────────┴─────────────────────────┘
                                 ▼
                          [6] Faz 4 (Editör)
                                 │
                    ┌────────────┼────────────┐
                    ▼            ▼            ▼
              [14] Asset    [7] Faz 5    [9] Faz 7
              Browser        (Fizik)     (Gelişmiş Grafik)
                    │            │        [BAĞIMSIZ DAL]
                    │            ▼
                    │      [8] Faz 6 (Networking)
                    │            │
                    │            ▼
                    │     [16] Interest Management
                    │            │
                    └──────┬─────┘
                           ▼
                [12] Karakter Kontrolcüsü
                    (Faz5 + Faz6 + Faz3/8 gerektirir)
                           │
                           ▼
              [13] Skeletal Animation
              (Karakter Kontrolcüsü + Asset Browser gerektirir)
                           │
                           ▼
              [15] Katmanlı Animasyon
              (Skeletal Animation'ın genişlemesi)
```

### B.1 Bu grafiğin okunuşu — kritik gözlemler

- **[3] Faz 1 Derinleştirme, aslında [2]'nin bir parçası olmalı.** Sohbette ayrı bir doküman olarak ele alındı ama gerçek geliştirmede, kalıtım zinciri ve karmaşık property tipleri **Faz 1 bitmeden** çözülmüş olmalı — çünkü [4], [5], [6] hepsi bunlara güveniyor. Bu, dokümanların yazılma sırasının doğal bir sonucu (basitten karmaşığa doğru anlatım), gerçek bağımlılık sırası değil.

- **[10] Script Timeout, [5]'in bir alt parçası olarak ele alınmalı.** Sohbette Faz 3-6 arası ertelendi (dokümanlarda da bu "erteleme notu" bilinçli olarak korundu, çünkü gerçek bir proje yönetimi dersi veriyor: bu tür bir güvenlik özelliğinin ertelenmesi, editör kullanıcılara açıldığı anda bir teknik borç yaratıyor). Ama gerçek geliştirmede, **Faz 3 bitmeden** (editör herkese açılmadan önce) bu mekanizma kurulmuş olmalı.

- **[9] Faz 7, tamamen bağımsız bir dal.** Diğer hiçbir sonraki sistem (Fizik, Networking, Karakter Kontrolcüsü) Faz 7'nin tamamlanmasına bağımlı değil — bu yüzden ekip büyükse paralel bir kişi/alt-ekip tarafından, ana zincirden bağımsız olarak sürekli iyileştirilebilir.

- **[11] Faz 8 (C#), aynı şekilde bağımsız bir dal.** Faz 1 Derinleştirme'den sonra herhangi bir noktada eklenebilir — [5]'e (Luau) teknik olarak bağımlı değil, sadece **karşılaştırma ve tutarlılık** için mantıksal olarak ondan sonra ele alınması pratikte daha kolay (Bölüm D, Faz 8 dokümanındaki "ne zaman hangisi" tablosu, Luau'nun zaten var olduğu bir referans noktası gerektiriyor).

- **[14] Asset Browser, [6]'nın doğal bir uzantısı** ama tam işlevi için (özellikle FBX/skeletal mesh import) [13]'teki importer koduna da ihtiyaç duyuyor — bu yüzden [14] ile [13] arasında **karşılıklı bir ilişki** var: Asset Browser'ın genel iskeleti [6]'dan hemen sonra kurulabilir, ama "skeletal mesh önizleme" gibi özellikler [13] tamamlanana kadar eksik kalır.

- **[16] Interest Management, [8]'in bir derinleştirmesi.** Küçük ölçekli bir test/prototip için [8]'in basit mesafe-tabanlı hali yeterli olabilir — [16] asıl **ölçeklenme** (100+ eşzamanlı oyuncu) aşamasında gerekli hale geliyor. Bu, erken MVP için atlanabilir, ama [12]'deki hareketli platform senaryosu net bir şekilde [16]'ya ihtiyaç duyuyor.

---

## Bölüm C — Önerilen Gerçek Geliştirme Sırası

Bölüm B'deki bağımlılık grafiğini, doğrusal ve pratik bir sıraya dönüştürüyoruz. Parantez içindeki süre tahminleri, orijinal Implementation Plan dokümanındaki (Doküman 1) tahminlerle tutarlı.

### Aşama 1 — Temel (Faz 0-1, birleşik)
```
[2] Faz 0/1 Teknik Detay  +  [3] Faz 1 Derinleştirme
```
Bu ikisi **tek bir geliştirme evresi** olarak ele alınmalı — reflection sistemi, kalıtım ve karmaşık tipler dahil, tam olgunlaşmadan bir sonraki adıma geçilmemeli (orijinal Implementation Plan'daki "Definition of Done" kontrol listesi burada her iki dokümanın kontrol listesini de kapsayacak şekilde genişletilmeli).

### Aşama 2 — Görsel + Script (paralel yürütülebilir)
```
[4] Faz 2 Render Pipeline     ┐
                                ├─→ [6] Faz 4 Editör
[5] Faz 3 Luau  +  [10] Script Timeout  ┘
```
Render ve Scripting, birbirinden bağımsız iki kişi/alt-ekip tarafından paralel geliştirilebilir (ikisi de sadece Aşama 1'e bağımlı). Editör, ikisinin de belirli bir olgunluğa ulaşmasını bekliyor (Viewport için Render, Script Editor/Play-Stop için Scripting gerekli).

**Not:** Script Timeout, Faz 3 ile birlikte, ayrı bir "sonradan eklenecek" iş kalemi olarak değil, Faz 3'ün bir parçası olarak planlanmalı.

### Aşama 3 — Editör Ekosistemi
```
[6] Faz 4 Editör  →  [14] Asset Browser
```

### Aşama 4 — Simülasyon Katmanı (paralel yürütülebilir)
```
[7] Faz 5 Fizik     ┐
                      ├─→ (ikisi birlikte) [12] Karakter Kontrolcüsü'nün önkoşulu
[8] Faz 6 Networking ┘
        │
        ▼
[16] Interest Management (ölçeklenme ihtiyacı doğduğunda)
```

### Aşama 5 — Oynanabilirlik
```
[12] Karakter Kontrolcüsü  →  [13] Skeletal Animation  →  [15] Katmanlı Animasyon
```
Bu üçü **doğrusal bir zincir** — her biri bir öncekinin üzerine inşa ediyor, paralelleştirilemez.

### Bağımsız Dallar (her aşamadan sonra herhangi bir noktada eklenebilir)
```
[9] Faz 7 Gelişmiş Grafik   — Faz 2'den sonra herhangi bir zaman, sürekli iyileştirme
[11] Faz 8 C# Desteği        — Aşama 1'den sonra herhangi bir zaman
```

### C.1 Görsel özet — tüm sıralama tek bakışta

```
Aşama 1: Temel Reflection
   │
   ├──────────────┬──────────────┐
   ▼              ▼              │
Aşama 2a:      Aşama 2b:         │
Render         Luau+Timeout      │
   │              │              │
   └──────┬───────┘              │
          ▼                      │
   Aşama 3: Editör ──────────────┤
          │                      │
          ▼                      │
   Aşama 3b: Asset Browser       │
          │                      │
          ├──────────────┐       │
          ▼              ▼       │
   Aşama 4a: Fizik   Aşama 4b: Networking
          │              │       │
          └──────┬───────┘       │
                 ▼                │
        Aşama 4c: Interest Mgmt   │
                 │                │
                 ▼                │
        Aşama 5: Karakter Kontrolcüsü
                 │
                 ▼
        Skeletal Animation
                 │
                 ▼
        Katmanlı Animasyon

  [Herhangi bir noktada, paralel dallar olarak:]
  Faz 7 (Gelişmiş Grafik) ── Faz 2'den sonra başlar, hiç bitmez
  Faz 8 (C# Desteği) ── Aşama 1'den sonra herhangi bir zaman
```

---

## Bölüm D — Tekrar Eden Mimari Temalar (Tüm Dokümanlarda Ortak)

Bu 16 dokümanı baştan sona okuyunca, aynı birkaç prensibin defalarca farklı bağlamlarda uygulandığı görülüyor. Bunları isimlendirmek, gelecekte yeni bir sistem eklerken "bu problemi daha önce nasıl çözmüştük?" sorusuna hızlı cevap vermeyi kolaylaştırır:

| Tema | İlk Ortaya Çıktığı Yer | Tekrar Kullanıldığı Yerler |
|---|---|---|
| **Reflection tek kayıt noktası** | Faz 1 | Faz 3 (Luau), Faz 4 (Editör UI), Faz 6 (Replication), Faz 8 (C#), Undo/Redo |
| **OOP ağaç ↔ düz dizi ayrımı ("dirty" deseni)** | Faz 2 (RenderProxy) | Faz 5 (PhysicsBodyHandle), Faz 6 (Replication dirty tracking) |
| **Ana thread'e geri taşıma (worker thread güvenliği)** | Faz 5 (Contact events) | Asset Browser (import pipeline) |
| **Karelere yayma (throttling)** | Faz 3 (Script Scheduler) | Asset Browser (thumbnail üretimi), Interest Management (initial sync) |
| **Kaynağa geri yazmama (feedback loop önleme)** | Faz 5 (Jolt→DataModel senkronizasyonu) | Asset Browser (`.meta` dosya izleme) |
| **Sadece "gerçeği" senkronize et, detayı yerel hesapla** | Faz 6 (Replication felsefesi) | Skeletal Animation (animasyon state senkronizasyonu) |
| **Hazır kütüphane + ince sarmalayıcı** | Faz 0 (bgfx, Jolt seçimi) | Faz 7 (meshoptimizer), Skeletal Animation (Assimp), Asset Browser (efsw), Karakter Kontrolcüsü (CharacterVirtual, ImGuizmo) |

**Bu tablonun pratik faydası:** Projeye yeni bir geliştirici katıldığında, veya sen aylar sonra bu dokümanlara geri döndüğünde, bu yedi temayı anlamak, tüm 16 dokümanın "aynı birkaç fikrin farklı kılıklarda tekrarı" olduğunu görmeyi sağlıyor — bu da yeni bir sistem tasarlarken hangi deseni referans alacağını hızla bulmanı kolaylaştırıyor.

---

## Bölüm E — Bilinçli Olarak Ertelenen / Kapsam Dışı Bırakılan Konular

Şeffaflık için, dokümanlar boyunca "bu, kapsam dışı" denilen noktaların bir listesi:

| Konu | Nerede Ertelendi | Neden |
|---|---|---|
| Ses sistemi (miniaudio/FMOD entegrasyonu) | Implementation Plan, Bölüm 2.1 | Kullanıcının isteğiyle şimdilik not olarak bırakıldı |
| Terrain/arazi sistemi | Implementation Plan kapsamında hiç yok | Kullanıcının isteğiyle şimdilik not olarak bırakıldı |
| Lokalizasyon | Hiçbir dokümanda yok | Kullanıcının isteğiyle şimdilik not olarak bırakıldı |
| Plugin/Marketplace ekosistemi | Hiçbir dokümanda yok | Kullanıcının isteğiyle şimdilik not olarak bırakıldı |
| Genel amaçlı N-kemikli IK (FABRIK/Jacobian) | Skeletal Animation, Bölüm D.2 | Two-Bone IK, kol/bacak için pratikte yeterli |
| Tam GPU-driven Nanite-tarzı LOD | Faz 7, Bölüm C.1 | Discrete LOD, "ayakları yere basan" hedef için yeterli; Nanite kalitesi Epic'in yıllar süren mühendisliğinin ürünü |
| Gerçek ray-traced GI (Lumen tarzı) | Faz 7, Bölüm A.1 | "Hafif motor" hedefiyle doğrudan çelişiyor |

---

## Bölüm F — Bu Noktadan Sonra Ne Yapılabilir

Üç somut seçenek:

1. **Gerçek koda başlamak:** Aşama 1'deki (Faz 0/1 + Derinleştirme) CMake iskeletini ve temel reflection sistemini gerçekten yazmaya başlamak — bu doküman setinin "teoriden pratiğe" geçiş noktası.
2. **Bölüm E'deki ertelenen konulardan birini** (Ses, Terrain, Lokalizasyon, Plugin ekosistemi) ileride, ilgili aşamaya gelindiğinde aynı derinlikte ele almak.
3. **Bu Master Index'i canlı bir doküman olarak güncel tutmak:** Gerçek geliştirme ilerledikçe, hangi "Definition of Done" maddelerinin tamamlandığını buraya işaretlemek — böylece bu doküman zamanla bir proje panosu (project board) işlevi de görebilir.

Hangisini istersin?
