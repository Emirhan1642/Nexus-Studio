# Oyun Motoru Geliştirme Planı
## Roblox Studio Hafifliği + Unreal Engine Gerçekçiliği

**Doküman amacı:** Bu plan, projeye sıfırdan başlayacak herhangi bir geliştiricinin (sen dahil, gelecekte katılacak biri dahil) hiçbir ek açıklamaya ihtiyaç duymadan projenin ne olduğunu, neden bu şekilde tasarlandığını ve hangi sırayla ilerleneceğini anlayabilmesi için yazılmıştır.

---

## 1. Vizyon ve Temel Prensipler

### 1.1 Ne inşa ediyoruz?

Üç ayrı motorun en güçlü yanlarını birleştiren, tek bir executable içinde çalışan (editör + runtime aynı process) bir oyun motoru:

| Özellik | Kaynak İlham | Neden |
|---|---|---|
| Anında açılma, hafif editör | Roblox Studio | Geliştirici sürtünmesi minimum olmalı |
| Sunucu-otoriteli çoklu oyuncu | Roblox Studio | Hile korumalı, tutarlı state |
| Fotogerçekçi render (GI, gölgeler) | Unreal Engine | Görsel kalite pazarlanabilir olmalı |
| Veri odaklı performans | Unity (DOTS) | Binlerce nesne aynı anda simüle edilebilmeli |
| Erişilebilir scripting | Roblox (Luau) | Yeni başlayan da yazabilmeli |
| Native dil desteği | Unreal/Unity (C++/C#) | Deneyimli geliştiriciler dışlanmamalı |

### 1.2 Temel mühendislik prensibi

> **"Tekerleği yeniden icat etme, ama tekerleğin üstüne oturacak arabayı sen tasarla."**

Bu, önceki konuşmalarda üzerinde durduğumuz kritik ayrımdır:

- **Hazır alınacaklar (asla sıfırdan yazılmayacak):** Fizik motoru çekirdeği, grafik API soyutlama katmanı, ağ transport katmanı, script dili VM'i
- **Sıfırdan yazılacaklar (projenin asıl değeri burada):** Nesne modeli (DataModel benzeri), reflection/binding sistemi, replication mantığı, editör-motor entegrasyonu, shader/lighting mimarisi, asset pipeline

### 1.3 Editör ve Motor İlişkisi (Kritik Mimari Karar)

**Karar: Editör ve motor aynı executable içinde çalışacak (in-process).**

Bunun anlamı: Editörü açtığında aslında motorun kendisini "edit modu"nda başlatmış oluyorsun. "Play" tuşuna bastığında ayrı bir program açılmıyor — aynı process içinde motor "play modu"na geçiyor. Bu, Unreal, Unity ve Godot'un da kullandığı yöntemdir.

**Neden bu şekilde olmalı:**
- Viewport (3D sahne görüntüsü) doğrudan GPU'ya yazan bir şeydir. Editör ayrı bir process olursa (örneğin web tabanlı bir arayüz), bu görüntüyü editöre aktarmak için ekstra bir katman (IPC, texture streaming) gerekir — bu hem performans kaybı hem de aylarca ekstra mühendislik demektir.
- Aynı process olduğu için Explorer panelinde bir obje seçtiğinde, o objenin C++ bellek adresine doğrudan erişilir. Ayrı process'te bu bir network mesajı olurdu.

---

## 2. Teknoloji Yığını (Technology Stack)

### 2.1 Katman katman özet tablo

| Katman | Teknoloji | Tipi | Gerekçe |
|---|---|---|---|
| **Çekirdek dil** | C++20 | Hazır (dil) | Endüstri standardı, tüm alt sistemler bu dille yazılır |
| **Grafik API soyutlaması** | bgfx | Hazır kütüphane | Vulkan/DirectX12/Metal'i tek API'dan yönetir, sıfırdan yazmak yıllar alır |
| **Fizik motoru** | Jolt Physics | Hazır kütüphane | Modern multi-thread mimari, açık kaynak, production-proven (Horizon Forbidden West) |
| **Scripting (Faz 1)** | Luau | Hazır VM + özel binding | Hızlı, hafif, sandboxlı, gradual typing |
| **Scripting (Faz 2)** | C# (Mono/CoreCLR embed) | Hazır runtime + özel binding | Deneyimli geliştiricileri projeye dahil eder |
| **Editör UI** | Dear ImGui | Hazır kütüphane | Aynı process, viewport entegrasyonu trivial, immediate-mode = az bakım |
| **Ağ transport** | GameNetworkingSockets (Valve) | Hazır kütüphane | UDP üzeri güvenilir, düşük gecikme |
| **Ağ mantığı (replication)** | — | Sıfırdan yazılacak | Projenin DataModel'ine özel, hazır çözüm işine yaramaz |
| **Ses** | miniaudio veya FMOD | Hazır kütüphane | Ses motoru yazmak zaman kaybı |
| **Derleme sistemi** | CMake + Ninja | Hazır araç | Cross-platform standardı |
| **Versiyon kontrolü** | Git + Git LFS | Hazır araç | Büyük binary assetler için LFS şart |

### 2.2 Neden bu sıralamayla karar verildi?

Her teknoloji seçiminde şu 3 soru soruldu:

1. **Bu bileşeni sıfırdan yazmak projenin ayırt edici değerine katkı sağlıyor mu?** (Hayırsa → hazır al)
2. **Piyasada bu işi yapan, production'da kanıtlanmış, açık kaynak bir çözüm var mı?** (Varsa → hazır al)
3. **Bu bileşen motorun "kimliğini" oluşturuyor mu?** (Evetse → sıfırdan yaz)

Fizik, render API'si, ses, scripting VM'i → 1. ve 2. soruya göre hazır alındı.
Nesne modeli, replication, reflection sistemi, shader mimarisi → 3. soruya göre sıfırdan yazılacak.

---

## 3. Proje Hiyerarşik Yapısı

### 3.1 Klasör / Modül Ağacı

```
GameEngine/
│
├── Engine/                          # Motor çekirdeği (C++)
│   │
│   ├── Core/                        # Temel altyapı
│   │   ├── Memory/                  # Custom allocator'lar
│   │   ├── Containers/              # Vector, HashMap vb. (ya da EASTL kullan)
│   │   ├── Reflection/              # ★ SIFIRDAN — nesne metadata sistemi
│   │   ├── Serialization/           # Sahne kaydetme/yükleme
│   │   ├── Threading/               # Job system, task scheduler
│   │   └── DataModel/               # ★ SIFIRDAN — Instance hiyerarşisi (Roblox benzeri)
│   │
│   ├── Renderer/                    # Grafik katmanı
│   │   ├── RHI/                     # bgfx wrapper (Render Hardware Interface)
│   │   ├── Shaders/                 # ★ SIFIRDAN — özel shader mimarisi
│   │   ├── Lighting/                # ★ SIFIRDAN — GI/gölgeleme sistemi
│   │   ├── Materials/               # Materyal sistemi
│   │   └── SceneGraph/              # Render edilecek objelerin organizasyonu
│   │
│   ├── Physics/                     # Fizik entegrasyonu
│   │   ├── JoltBindings/            # Jolt Physics wrapper
│   │   └── Constraints/             # ★ SIFIRDAN — Roblox tarzı constraint API'leri
│   │
│   ├── Scripting/                   # Scripting katmanı
│   │   ├── LuauRuntime/             # Luau VM entegrasyonu
│   │   ├── CSharpRuntime/           # (Faz 2) C# entegrasyonu
│   │   └── APIBindings/             # ★ SIFIRDAN — Reflection → Script köprüsü
│   │
│   ├── Networking/                  # Ağ katmanı
│   │   ├── Transport/               # GameNetworkingSockets wrapper
│   │   └── Replication/             # ★ SIFIRDAN — DataModel senkronizasyonu
│   │
│   ├── Assets/                      # Asset pipeline
│   │   ├── Importers/               # FBX, PNG, WAV vb. import
│   │   ├── AssetDatabase/           # ★ SIFIRDAN — asset referans sistemi
│   │   └── Streaming/               # Büyük dünyalar için asset streaming
│   │
│   └── Audio/                       # Ses sistemi (miniaudio wrapper)
│
├── Editor/                          # Editör uygulaması (Engine'i kullanır)
│   ├── UI/                          # ImGui panelleri
│   │   ├── Viewport/                # 3D sahne görüntüsü
│   │   ├── Explorer/                # Hiyerarşi paneli (DataModel ağacı)
│   │   ├── Properties/              # Seçili obje özellikleri
│   │   ├── ScriptEditor/            # Kod editörü (syntax highlighting)
│   │   └── AssetBrowser/            # Proje dosyaları paneli
│   └── Tools/                       # Terrain sculpting, CSG editörü vb.
│
├── Runtime/                         # Oyunun kendisi (paketlenmiş build)
│   └── Player/                      # Editörsüz, sadece oynatma modu
│
├── ThirdParty/                      # Tüm hazır kütüphaneler burada
│   ├── bgfx/
│   ├── JoltPhysics/
│   ├── luau/
│   ├── imgui/
│   └── GameNetworkingSockets/
│
└── Tools/                           # Build scriptleri, CI/CD, asset converter'lar
```

### 3.2 Modüller arası bağımlılık hiyerarşisi

Aşağıdaki, hangi modülün hangisine bağımlı olduğunu gösterir. **Ok yönü "bağımlıdır" anlamına gelir.** Bu sıralama aynı zamanda geliştirme sırasını da işaret eder — alttaki modüller önce tamamlanmalı.

```
                     ┌─────────────┐
                     │   Editor    │  (en üstte, her şeye bağımlı)
                     └──────┬──────┘
                            │
              ┌─────────────┼─────────────┐
              ▼             ▼             ▼
      ┌──────────────┐ ┌─────────┐ ┌─────────────┐
      │  Networking  │ │ Assets  │ │  Scripting  │
      └───────┬──────┘ └────┬────┘ └──────┬──────┘
              │              │              │
              └──────────────┼──────────────┘
                             ▼
                     ┌───────────────┐
                     │  DataModel    │  (Instance hiyerarşisi)
                     └───────┬───────┘
                             │
              ┌──────────────┼──────────────┐
              ▼              ▼              ▼
      ┌──────────────┐ ┌──────────┐ ┌─────────────┐
      │   Renderer   │ │ Physics  │ │    Audio    │
      └──────┬───────┘ └────┬─────┘ └─────────────┘
             │               │
             └───────┬───────┘
                     ▼
             ┌───────────────┐
             │  Core (Reflection, │
             │  Memory, Threading)│
             └───────────────┘  (en altta, hiçbir şeye bağımlı değil)
```

**Bu diyagramın pratik anlamı:** Core ve DataModel'i tamamlamadan Renderer'a geçmek mantıksız — çünkü Renderer'ın render edeceği şey zaten DataModel'deki objelerdir. Bu yüzden Faz 1 tamamen Core + DataModel'e odaklanıyor (aşağıda detaylı).

---

## 4. Fazlı Yol Haritası

> **Genel süre tahmini:** "Ayakları yere basan" bir MVP için 12-18 ay, tam özellikli bir motor için 3-5 yıl (bu, 1-3 kişilik bir ekip varsayımıyla hesaplanmıştır — büyük stüdyolar 100+ mühendisle çalışır).

### FAZ 0 — Temel Altyapı (2-3 ay)

**Hedef:** Boş bir pencere açan, içine bir üçgen çizebilen, hiçbir oyun mantığı olmayan iskelet.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Proje derleme sistemi kurulumu | CMake + Ninja | Tek komutla tüm platformlar derlenebiliyor |
| Pencere/input yönetimi | GLFW veya SDL3 | Pencere açılıyor, klavye/mouse okunuyor |
| bgfx entegrasyonu | bgfx | Ekrana renkli bir üçgen çiziliyor |
| Temel memory allocator | Sıfırdan | Custom new/delete, memory tracking |
| Job system iskeleti | Sıfırdan (basit thread pool) | Görevler paralel thread'lere dağıtılabiliyor |
| Log sistemi | Sıfırdan (basit) | Konsola ve dosyaya log yazılabiliyor |

**Bu fazın sonunda:** Hiçbir "oyun" özelliği yok ama proje derleniyor, bir pencere açılıyor, temel altyapı sağlam.

---

### FAZ 1 — Çekirdek Nesne Sistemi (3-4 ay)

**Hedef:** Roblox'un `workspace.Part` deneyimine benzer bir DataModel + Reflection sistemi.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Reflection sistemi tasarımı | Sıfırdan | C++ sınıfları çalışma zamanında "tanınabiliyor" (`registerClass`, `registerProperty`) |
| Instance/DataModel ağacı | Sıfırdan | Hiyerarşik obje yapısı (`Parent`, `Children`, `FindFirstChild` gibi API'ler) |
| Serileştirme | Sıfırdan (reflection üzerine) | Sahne kaydedilip yüklenebiliyor (JSON veya binary format) |
| Temel geometrik primitive'ler | Sıfırdan | Küp, küre, silindir gibi ilkel şekiller (Roblox Part benzeri) |
| Basit CSG desteği (opsiyonel, ileri faz de olabilir) | Sıfırdan veya açık kaynak (örn. Manifold kütüphanesi) | Union/Intersect/Negate işlemleri |

**Kritik tasarım kararı burada verilir:** Reflection API'sinin biçimi (`registerClass<Part>("Part").property("Position", &Part::position)` gibi bir syntax) Faz 3'teki hem Luau hem C# binding'lerinin temelini oluşturacak. Bu yüzden bu faz aceleye getirilmemeli.

**Bu fazın sonunda:** Kod ile bir "Part" oluşturup, ağaca ekleyip, kaydedip tekrar yükleyebiliyorsun — ama henüz görsel olarak göremiyorsun (render entegrasyonu Faz 2'de).

---

### FAZ 2 — Render Pipeline (4-6 ay)

**Hedef:** DataModel'deki objeleri ekranda görebilmek, temel ışıklandırma.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Scene graph → render queue köprüsü | Sıfırdan | DataModel'deki objeler otomatik olarak çiziliyor |
| Kamera sistemi | Sıfırdan | Free-cam, perspective/orthographic |
| Temel materyal sistemi (PBR) | Sıfırdan (bgfx shader'ları üzerine) | Metallic/Roughness workflow |
| Forward veya deferred rendering seçimi | Sıfırdan | Çoklu ışık kaynağı desteklenebiliyor |
| Temel gölgeler (shadow mapping) | Sıfırdan | Directional/point light gölgeleri |
| Skybox / ortam ışığı | Sıfırdan | Basit ambient/IBL |

**Not:** Nanite/Lumen seviyesi gerçekçilik bu fazda hedeflenmiyor. Bu faz "çalışan, PBR temelli, gölgeli bir render pipeline" hedefliyor. Gelişmiş GI (global illumination) Faz 5'te ele alınacak.

**Bu fazın sonunda:** Editörsüz bir test uygulamasında, oluşturduğun objeler ışıklandırılmış şekilde ekranda görünüyor.

---

### FAZ 3 — Scripting Entegrasyonu (3-4 ay)

**Hedef:** Luau ile DataModel'i kontrol edebilmek.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Luau VM gömme (embedding) | Luau (hazır) | `.lua` script dosyaları çalıştırılabiliyor |
| Reflection → Luau köprüsü | Sıfırdan (Faz 1'deki reflection üzerine) | `part.Position = Vector3.new(0,10,0)` gibi kod çalışıyor |
| Sandboxing / güvenlik | Sıfırdan | Scriptler dosya sistemine, ağa keyfi erişemiyor |
| Event sistemi | Sıfırdan | `part.Touched:Connect(function() ... end)` gibi API |
| Script debugging araçları | Sıfırdan (temel) | Hata mesajları, satır numaraları doğru gösteriliyor |

**Bu fazın sonunda:** Bir Luau scripti yazıp, bir objeyi hareket ettirebiliyorsun. Bu, "ayakları yere basan proje" hedefinin gerçek anlamda karşılandığı nokta.

---

### FAZ 4 — Editör (4-5 ay, Faz 1-3 ile paralel de yürütülebilir)

**Hedef:** Roblox Studio benzeri bir arayüz.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| ImGui entegrasyonu | Dear ImGui (hazır) | Panel tabanlı arayüz iskeleti |
| Viewport paneli | Sıfırdan (bgfx render target → ImGui texture) | 3D sahne editör içinde görünüyor |
| Explorer paneli | Sıfırdan (DataModel ağacını gösterir) | Hiyerarşi tıklanabilir, sürüklenebilir |
| Properties paneli | Sıfırdan (Reflection sistemini okur) | Seçili objenin özellikleri düzenlenebiliyor |
| Gizmo sistemi (move/rotate/scale) | Sıfırdan veya ImGuizmo (hazır) | Objeler fare ile taşınabiliyor |
| Script editörü | Sıfırdan (basit) veya CodeMirror gömme | Syntax highlighting'li kod editörü |
| Undo/Redo sistemi | Sıfırdan | Ctrl+Z çalışıyor |

**Bu fazın sonunda:** Kod yazmadan, tamamen mouse ile bir sahne kurup, script ekleyip, Play tuşuna basıp test edebiliyorsun. **Bu nokta "MVP" (Minimum Viable Product) olarak kabul edilebilir.**

---

### FAZ 5 — Fizik Entegrasyonu (2-3 ay)

**Hedef:** Jolt Physics ile gerçekçi fizik davranışı.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Jolt Physics entegrasyonu | Jolt (hazır) | Objeler yerçekimi ile düşüyor, çarpışıyor |
| Collision shape sistemi | Jolt üzerine wrapper | Küp, küre, mesh collision |
| Constraint API'leri (Roblox WeldConstraint benzeri) | Sıfırdan | Objeler birbirine bağlanabiliyor |
| Fizik-render senkronizasyonu | Sıfırdan | Fizik simülasyonu ile görsel pozisyon senkron |
| Script'ten fizik kontrolü | Sıfırdan (Faz 3 binding'i genişletilir) | `part.Velocity = Vector3.new(...)` gibi API |

---

### FAZ 6 — Networking / Multiplayer (3-4 ay)

**Hedef:** Roblox tarzı sunucu-otoriteli çoklu oyuncu.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| GNS entegrasyonu | GameNetworkingSockets (hazır) | İstemci-sunucu bağlantısı kuruluyor |
| DataModel replication | Sıfırdan | Sunucudaki değişiklikler istemciye otomatik yansıyor |
| Client-side prediction | Sıfırdan | Hareket gecikme hissettirmiyor |
| RemoteEvent benzeri API | Sıfırdan (Faz 3 üzerine) | Script'ten sunucu-istemci mesajlaşması |

---

### FAZ 7 — Gelişmiş Grafik Kalitesi (6-12 ay, sürekli iyileştirme)

**Hedef:** "Unreal kadar gerçekçi" hedefine yaklaşmak.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| Gerçek zamanlı GI (Global Illumination) | Sıfırdan (örn. voxel cone tracing veya SDF tabanlı) | Dolaylı ışıklandırma |
| Post-processing pipeline | Sıfırdan (bgfx üzerine) | Bloom, tonemap, ambient occlusion |
| LOD sistemi (Nanite'a ilham, tam kopyası değil) | Sıfırdan | Uzak objeler otomatik basitleşiyor |
| Gelişmiş materyal editörü | Sıfırdan (node-based, editörde) | Görsel shader oluşturma |

**Not:** Bu faz, Nanite/Lumen seviyesine "ilham alarak yaklaşmak" anlamına gelir — bire bir aynısını yazmak, Epic Games'in onlarca mühendisinin yıllarca çalıştığı bir iştir. Gerçekçi hedef: "iyi görünen, kabul edilebilir performanslı bir GI çözümü."

---

### FAZ 8 — C# Desteği (2-3 ay, opsiyonel/paralel)

**Hedef:** Deneyimli geliştiricilerin C# ile de yazabilmesi.

| Görev | Teknoloji | Çıktı |
|---|---|---|
| .NET runtime embedding | CoreCLR hosting API (hazır) | C# kodu motor içinde çalıştırılabiliyor |
| Reflection → C# köprüsü | Sıfırdan (Faz 1'deki reflection üzerine, Faz 3'e paralel mantık) | Aynı `Part` nesnesi C#'tan da erişilebiliyor |
| C# proje şablonları | Sıfırdan | Visual Studio/Rider ile motor projesine bağlanabiliyor |

---

## 5. Zaman Çizelgesi Özeti

```
Ay:     0    3    6    9    12   15   18   21   24   27   30   33   36+
        │    │    │    │    │    │    │    │    │    │    │    │    │
Faz 0   ████
Faz 1        ████████
Faz 2             ████████████
Faz 3                       ████████████
Faz 4                  ████████████████        (Faz 1-3 ile paralel başlar)
                                          │
                                          ▼
                                    ★ MVP NOKTASI ★
                                    (Ay ~18-20)
                                          │
Faz 5                                    ████████
Faz 6                                         ████████████
Faz 7                                                   ████████████████████
Faz 8                                                        ████████
```

**★ MVP (Minimum Viable Product) noktası** Faz 4'ün sonunda, yaklaşık 18-20. ayda geliyor. Bu noktada: bir editörün içinde, mouse ile sahne kurup, Luau ile script yazıp, sahneyi test edebiliyorsun — henüz multiplayer ve fotogerçekçi grafik yok ama "ayakları yere basan" hedefi tam anlamıyla karşılanmış oluyor.

---

## 6. Riskler ve Dikkat Edilmesi Gerekenler

| Risk | Açıklama | Önlem |
|---|---|---|
| **Reflection sistemi hatalı tasarlanırsa** | Faz 3, 6 ve 8'in tümü bu sisteme bağımlı — sonradan değiştirmek üç binding'i de bozar | Faz 1'de reflection API'sini tasarlarken Luau *ve* C# binding'inin nasıl çalışacağını baştan düşün, ikisi için de yeterli olacak şekilde tasarla |
| **Render pipeline'a çok erken "gerçekçilik" eklemeye çalışmak** | Faz 2'de Nanite/Lumen seviyesini hedeflemek, temel her şeyi geciktirir | Faz 2'de sade PBR + gölge yeterli, gelişmiş GI Faz 7'ye bırakılmalı |
| **Scope creep (kapsam genişlemesi)** | "Roblox kadar hafif + Unreal kadar gerçekçi" hedefi çok geniş, her fazda yeni fikir eklemek isteği doğar | Her faz için yukarıdaki tabloda tanımlı görevler dışına çıkılmamalı, yeni fikirler bir "backlog" listesine yazılıp sonraki faz için saklanmalı |
| **Tek kişi/küçük ekip ile 3-5 yıllık plan** | Büyük motorların arkasında yüzlerce mühendis var | Gerçekçi beklenti: MVP (Faz 4 sonu) ulaşılabilir bir hedef, "Unreal kalitesi" (Faz 7) uzun vadeli, sürekli iyileştirilen bir hedef olarak görülmeli |
| **Üçüncü parti kütüphane versiyonlama sorunları** | bgfx, Jolt, Luau gibi kütüphaneler zamanla güncellenir, breaking change riski var | Her kütüphaneyi Git submodule olarak sabit bir commit'e kilitle, güncellemeleri kontrollü yap |

---

## 7. Sonraki Adım Önerisi

Bu plan, üst seviye yol haritasını tanımlıyor. Bir sonraki mantıklı adım **Faz 0 ve Faz 1'in teknik detaylarına inmek**: 

- Reflection sisteminin C++ API tasarımı (`registerClass`, `registerProperty` gibi fonksiyonların imzaları)
- DataModel'in bellek yönetimi stratejisi (reference counting mı, garbage collection mı?)
- CMake proje yapısının somut kurulumu

Bu konulardan hangisiyle devam etmek istersin?
