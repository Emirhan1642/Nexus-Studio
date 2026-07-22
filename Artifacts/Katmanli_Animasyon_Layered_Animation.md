# Katmanlı Animasyon (Layered Animation) — Teknik Derinlemesine İnceleme
## Bone Mask, Additive Blending ve Çoklu Katman Kompozisyonu

Bu doküman, Skeletal Animation dokümanının Bölüm B.3'ünde bilinçli olarak ertelenen konuyu ele alıyor: Bir karakterin **aynı anda** koşarken hem el sallayabilmesi, hem nişan alırken belden yukarısını bağımsız döndürebilmesi. Hedef: `AnimationPlayer`'ın tek-klip modelinden, birden fazla animasyonun vücut bölgelerine göre birleştirildiği bir kompozisyon sistemine geçmek.

---

## Bölüm A — Sorunun Netleştirilmesi: Neden Tek Katman Yetmiyor

### A.1 Somut senaryo

Bir oyuncu koşarken (`Run` klibi, tüm vücut) aynı zamanda bir öğeyi kullanmak için elini sallıyor (`Wave` klibi, sadece kol/omuz bone'ları) diyelim. Skeletal Animation dokümanındaki `AnimationPlayer` (Bölüm B), tek seferde **sadece bir** klip çalıştırabiliyordu — `play()` çağrısı önceki klibi tamamen değiştiriyordu. `Run` çalarken `Wave`'i `play()` ile başlatmak, bacakların da `Wave`'in (muhtemelen T-pose veya idle) pozuna geçmesine yol açar — istemediğimiz sonuç bu.

**İhtiyaç:** Vücudun farklı bölgelerinin (bel altı, bel üstü, sadece sağ kol vb.) **aynı anda, birbirinden bağımsız** farklı animasyonlardan beslenebilmesi.

---

## Bölüm B — Bone Mask: Vücudu Bölgelere Ayırmak

### B.1 Temel veri yapısı

```cpp
// Engine/Animation/BoneMask.h

class BoneMask {
public:
    // Her bone için 0.0 (bu katman bu bone'u hiç etkilemiyor) ile 1.0 (tam etkiliyor) arası bir ağırlık
    std::vector<float> boneWeights; // skeleton.bones.size() kadar eleman

    static BoneMask createFullBody(const Skeleton& skeleton) {
        BoneMask mask;
        mask.boneWeights.assign(skeleton.bones.size(), 1.0f);
        return mask;
    }

    // Belirli bir bone'dan başlayıp, tüm alt hiyerarşiyi (çocuk bone'ları) dahil eden bir maske üretir
    static BoneMask createFromRoot(const Skeleton& skeleton, const std::string& rootBoneName, float weight = 1.0f) {
        BoneMask mask;
        mask.boneWeights.assign(skeleton.bones.size(), 0.0f);

        int rootIdx = skeleton.findBoneIndex(rootBoneName);
        if (rootIdx == -1) return mask;

        std::function<void(int)> markSubtree = [&](int boneIdx) {
            mask.boneWeights[boneIdx] = weight;
            for (int i = 0; i < (int)skeleton.bones.size(); i++)
                if (skeleton.bones[i].parentIndex == boneIdx) markSubtree(i); // Recursive — tüm alt zincir
        };
        markSubtree(rootIdx);
        return mask;
    }
};
```

**Kullanım örneği:** `BoneMask::createFromRoot(skeleton, "Spine1")`, omurga ve üzerindeki (kollar, göğüs, kafa) tüm bone'ları 1.0 ağırlıkla işaretler, bacaklar 0.0 kalır — "bel üstü" maskesi.

### B.2 Neden bazı bone'larda "yumuşak" geçiş (0.0-1.0 arası) gerekiyor

Omurga gibi bir bone'da maskeyi keskin bir şekilde 0'dan 1'e sıçratmak (`Spine1` = 1.0, `Spine0` = 0.0), bel bölgesinde görsel bir "kırılma" yaratır — üst gövde bir animasyondan, alt gövde başka bir animasyondan beslendiğinde omurga eklem noktasında ani bir açı değişimi oluşur. Bu yüzden pratikte omurga zincirinin birkaç bone'unda **kademeli** bir geçiş (örn. `Spine0`=0.2, `Spine1`=0.6, `Spine2`=1.0) tanımlanır — bu, "mask feathering" olarak bilinir.

```cpp
void BoneMask::applyFeathering(const Skeleton& skeleton, const std::vector<std::string>& transitionBones) {
    float step = 1.0f / (transitionBones.size() + 1);
    for (int i = 0; i < (int)transitionBones.size(); i++) {
        int idx = skeleton.findBoneIndex(transitionBones[i]);
        if (idx != -1) boneWeights[idx] = step * (i + 1);
    }
}
```

---

## Bölüm C — Animation Layer: Bir Maskenin ve Bir Blend Modunun Birleşimi

### C.1 İki blend modu: Override vs Additive

| Mod | Ne yapar | Örnek kullanım |
|---|---|---|
| **Override** | Bu katmandaki bone'lar, alttaki katmanların pozunu **tamamen değiştirir** | "Bel üstü Wave animasyonu oynat" — kollar artık Run'dan değil Wave'den beslenir |
| **Additive** | Bu katmandaki poz, alttaki katmanın pozuna **eklenir** (fark olarak) | "Nefes alma" gibi ince bir hareketi, hangi animasyon çalıyor olursa olsun üstüne bindirmek |

### C.2 AnimationLayer veri yapısı

```cpp
// Engine/Animation/AnimationLayer.h

enum class LayerBlendMode { Override, Additive };

struct AnimationLayer {
    std::string name;
    AnimationPlayer player;      // Skeletal Animation dokümanı, Bölüm B'deki AnimationPlayer — her katmanın kendi çalma durumu var
    BoneMask mask;
    LayerBlendMode blendMode;
    float layerWeight = 1.0f;    // Tüm katmanın genel etkisi (fade in/out için) — mask'tan bağımsız, ek bir çarpan
    int priority = 0;            // Aynı bone'u etkileyen birden fazla katman varsa, kim üste yazar (Bölüm D)
};
```

### C.3 Additive animasyonun matematiksel temeli — "fark" pozu

Additive bir klip, ham bir animasyon klibi değil, **bir referans pozdan farkı** temsil eder. Bu fark, import sırasında önceden hesaplanır:

```cpp
// Import zamanında (Skeletal Animation dokümanı, Bölüm E'ye ek bir adım)
AnimationClip convertToAdditive(const AnimationClip& sourceClip, const AnimationClip& referenceClip) {
    AnimationClip additiveClip = sourceClip; // Zaman damgaları, isim vb. kopyalanır

    for (auto& track : additiveClip.boneTracks) {
        Matrix4 referencePose = referenceClip.sampleBone(track.boneIndex, 0.0f); // Genelde referansın ilk karesi (bind pose) kullanılır

        for (int i = 0; i < (int)track.times.size(); i++) {
            Matrix4 sourcePose = Matrix4::fromTRS(track.positions[i], track.rotations[i], track.scales[i]);
            Matrix4 delta = referencePose.inverse() * sourcePose; // ★ "Referansa göre fark" transformu
            track.positions[i] = delta.getTranslation();
            track.rotations[i] = delta.getRotation();
        }
    }
    return additiveClip;
}
```

**Runtime'da uygulanışı:** Bir additive katman uygulanırken, o bone'un alttaki (override) katmanlardan gelen pozu ile bu "fark" transformu **çarpılır** (rotasyonlar için) veya **eklenir** (pozisyonlar için) — böylece "nefes alma" gibi ince bir hareket, karakter Run'da olsun Idle'da olsun, altındaki her animasyonun üzerine tutarlı şekilde bindirilebilir.

---

## Bölüm D — Katman Kompozisyonu: `AnimationLayerStack`

### D.1 Katmanların birleştirilme sırası

```cpp
// Engine/Animation/AnimationLayerStack.h

class AnimationLayerStack {
public:
    std::vector<AnimationLayer> layers; // Öncelik sırasına göre dizilmiş (priority düşükten yükseğe)

    std::vector<Matrix4> evaluate(const Skeleton& skeleton, float deltaTime) {
        // 1. Başlangıç: en alt katman (genelde "Base Layer" — tam vücut Idle/Walk/Run)
        std::vector<Matrix4> result = layers[0].player.evaluate(skeleton, deltaTime);

        // 2. Sonraki katmanlar, öncelik sırasına göre üstüne bindirilir
        for (int i = 1; i < (int)layers.size(); i++) {
            AnimationLayer& layer = layers[i];
            std::vector<Matrix4> layerPose = layer.player.evaluate(skeleton, deltaTime);

            for (int boneIdx = 0; boneIdx < (int)skeleton.bones.size(); boneIdx++) {
                float effectiveWeight = layer.mask.boneWeights[boneIdx] * layer.layerWeight;
                if (effectiveWeight <= 0.0f) continue; // Bu katman bu bone'u etkilemiyor — atla (performans)

                if (layer.blendMode == LayerBlendMode::Override) {
                    result[boneIdx] = blendTransforms(result[boneIdx], layerPose[boneIdx], effectiveWeight);
                } else { // Additive
                    result[boneIdx] = applyAdditive(result[boneIdx], layerPose[boneIdx], effectiveWeight);
                }
            }
        }
        return result;
    }

private:
    Matrix4 applyAdditive(const Matrix4& basePose, const Matrix4& additiveDelta, float weight) {
        Quaternion blendedDelta = Quaternion::identity().slerp(additiveDelta.getRotation(), weight);
        return basePose * Matrix4::fromRotation(blendedDelta); // Fark, ağırlıklandırılıp temel poza uygulanır
    }
};
```

**Performans notu:** `if (effectiveWeight <= 0.0f) continue;` satırı önemsiz görünse de kritik — bir "sadece sağ el" maskesi, iskeletin geri kalan ~95 bone'u için bu döngüyü anında atlıyor. Bone sayısı arttıkça (karmaşık yüz animasyonlarında yüzlerce bone olabilir) bu erken çıkış, gereksiz matris işlemlerinden kaçınıyor.

### D.2 Aynı bone'u etkileyen birden fazla katman — `priority` alanının rolü

Eğer hem "Bel Üstü — Wave" (priority=1) hem "Sağ El — Point" (priority=2) katmanları aynı anda aktifse ve ikisi de sağ eli etkiliyorsa, `layers` dizisindeki sıralama (priority'ye göre) belirleyici oluyor — döngü sırayla ilerlediği için, en son işlenen (en yüksek priority'li) katman, o bone için "son sözü söylüyor" (override modunda). Bu, çoğu oyun motorunun animasyon katman sisteminde (Unreal'ın Anim Blueprint'indeki katman önceliği gibi) kullanılan tanıdık bir desendir.

---

## Bölüm E — Humanoid ile Entegrasyon

### E.1 Karakter Kontrolcüsü dokümanındaki `AnimationController`'ın genişletilmesi

```cpp
// Engine/Animation/CharacterAnimator.h (Karakter Kontrolcüsü dokümanı, Bölüm E'nin devamı)

class CharacterAnimator {
public:
    void initialize(const Skeleton& skeleton) {
        AnimationLayer baseLayer{"Base", {}, BoneMask::createFullBody(skeleton), LayerBlendMode::Override, 1.0f, 0};
        AnimationLayer upperBodyLayer{"UpperBody", {}, BoneMask::createFromRoot(skeleton, "Spine1"), LayerBlendMode::Override, 0.0f, 1};
        AnimationLayer breathingLayer{"Breathing", {}, BoneMask::createFullBody(skeleton), LayerBlendMode::Additive, 1.0f, 2};

        layerStack.layers = {baseLayer, upperBodyLayer, breathingLayer};
    }

    // HumanoidStateMachine hâlâ Base Layer'ı kontrol ediyor — Karakter Kontrolcüsü dokümanı, Bölüm C.1 ile birebir aynı
    void onHumanoidStateChanged(HumanoidState newState) {
        AnimationLayer& base = layerStack.layers[0];
        switch (newState) {
            case HumanoidState::Walking: base.player.play(walkClip, 0.2f); break;
            case HumanoidState::Jumping: base.player.play(jumpClip, 0.1f); break;
            // ...
        }
    }

    // ★ Yeni: script'ten (Faz 3/8) tetiklenen üst-vücut hareketleri
    void playUpperBodyGesture(AnimationClip* clip) {
        AnimationLayer& upper = layerStack.layers[1];
        upper.player.play(clip, 0.15f);
        upper.layerWeight = 1.0f; // Katmanı aktive et
    }

private:
    AnimationLayerStack layerStack;
};
```

### E.2 Script API'sinin görünümü — Reflection ile tutarlılık

Faz 1'den beri kurduğumuz desenle tutarlı kalarak, bu yeni sistem de reflection'a bağlanıyor:

```cpp
ClassBuilder<AnimationTrack>("AnimationTrack") // Asset Browser dokümanı, Bölüm E.2'de tanımlanmıştı
    .base("Instance")
    .property("Priority", &AnimationTrack::priority)          // ★ Yeni
    .property("AdditiveBlendMode", &AnimationTrack::isAdditive) // ★ Yeni
    .method("Play", &AnimationTrack::play)
    .method("Stop", &AnimationTrack::stop);
```

Bu sayede bir Luau/C# scripti şunu yazabiliyor: `waveTrack.Priority = 1; waveTrack:Play()` — ve sistem otomatik olarak doğru katmana, doğru maskeyle yerleştiriyor. Script, `AnimationLayerStack`'in iç mimarisinden tamamen habersiz kalıyor; sadece `AnimationTrack` nesnesiyle konuşuyor.

---

## Bölüm F — Networking ile Etkileşim: Çoklu Katman Replikasyonu

### F.1 Skeletal Animation dokümanının Bölüm F'sinin genişletilmesi

Tek katmanlı sistemde, replicate edilen veri `{clipName, startTime, blendDuration}` idi. Katmanlı sistemde, **her aktif katman için ayrı** bir bu tür kayıt gerekiyor:

```cpp
struct LayeredAnimationReplication {
    InstanceId humanoidId;
    struct LayerState {
        std::string layerName;      // "Base", "UpperBody" vb.
        std::string clipName;
        float clipStartServerTime;
        float layerWeight;          // Fade in/out durumu da senkronize edilmeli
    };
    std::vector<LayerState> activeLayers; // Genelde 2-4 eleman — Base her zaman var, diğerleri opsiyonel
};
```

**Bant genişliği notu:** Bir karakterin çoğu zaman sadece Base Layer'ı aktif olur (basit yürüme/koşma) — `UpperBody` veya `Breathing` gibi katmanlar sadece tetiklendiğinde pakete eklenir, boş/pasif katmanlar hiç gönderilmez. Bu, Faz 6'daki genel "sadece dirty olanı gönder" prensibinin burada da geçerli olması.

---

## Bölüm G — "Definition of Done" Kontrol Listesi

- [ ] `BoneMask::createFromRoot()` doğru çalışıyor — belirtilen bone'dan itibaren tüm alt hiyerarşiyi doğru işaretliyor
- [ ] Mask feathering (Bölüm B.2) uygulandığında, bel bölgesinde animasyon geçişi görsel olarak yumuşak (kırılma yok)
- [ ] Bir karakter Base Layer'da `Run` çalarken, UpperBody Layer'da `Wave` çalabiliyor — bacaklar Run'dan, kollar Wave'den besleniyor
- [ ] Additive katman (örn. Breathing) doğru çalışıyor — hangi Base animasyon çalıyor olursa olsun (Idle, Walk, Run) üstüne tutarlı şekilde bindiriliyor
- [ ] Additive klip dönüşümü (Bölüm C.3) import sırasında doğru hesaplanıyor — referans poza göre fark doğru çıkarılıyor
- [ ] İki katman aynı bone'u etkilediğinde, `priority` alanına göre doğru katman "kazanıyor"
- [ ] `effectiveWeight <= 0.0f` erken-çıkış optimizasyonu doğrulanmış — düşük etkili bone'larda gereksiz hesaplama yapılmıyor (profiling ile)
- [ ] Script'ten `track.Priority` ve `AdditiveBlendMode` ayarlanabiliyor, `Play()`/`Stop()` doğru katmana yerleşiyor
- [ ] Networking: sadece aktif katmanlar replicate ediliyor, boş/pasif katmanlar pakete girmiyor
- [ ] Performans testi: 4-5 aktif katmanlı, ~80 bone'lu bir karakterde, katman kompozisyonu her karede kabul edilebilir sürede tamamlanıyor (tek katmana kıyasla ek maliyet ölçülmüş)

---

## Sonraki Adım Önerisi

Katmanlı animasyon ile birlikte, animasyon sistemi Roblox'un ötesine geçen bir esneklik kazandı (Roblox'un R15 rig sistemi bu düzeyde bir katmanlamayı yerleşik olarak sunmuyor). Kalan bekleyen tek konu:

1. **Interest Management derinleştirmesi:** Faz 6'dan beri bekliyor — artık Karakter Kontrolcüsü ve Layered Animation'ın networking tarafları da bu sisteme bağımlı hale geldiğine göre, önceliği iyice arttı.

Bunun dışında, tüm fazlar ve bekleyen dallanma noktaları artık tamamlanmış durumda. İstersen buradan, projenin genel bir "durum özeti" haritasını çıkarıp hangi dokümanların hangi sıralamayla gerçek geliştirmeye başlanacağını netleştirebiliriz, ya da Interest Management ile devam edebiliriz.
