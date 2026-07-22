# Tam Skeletal Animation Sistemi — Teknik Derinlemesine İnceleme
## Bone Hiyerarşisi, Skinning, Blend Tree ve IK

Bu doküman, Karakter Kontrolcüsü dokümanının Bölüm E'sinde kapsam dışı bırakılan konuyu ele alıyor: `AnimationController::onHumanoidStateChanged()`'in `playAnimation("Walk", 0.2f)` çağrısının arkasında gerçekte ne olduğu. Hedef: Bir `.fbx` dosyasından import edilen bir animasyon klibinin, GPU'da bir karakterin iskeletini gerçek zamanlı deforme etmesi.

---

## Bölüm A — Temel Kavramlar ve Veri Modeli

### A.1 Skeleton, Bone, Pose — üç temel kavram

- **Skeleton (İskelet):** Bir karakterin bone'larının (kemik) hiyerarşik yapısı — hangi bone'un ebeveyni hangisi (`Hand` → `Forearm` → `UpperArm` → `Shoulder` → `Spine`...). Bu, referans/bağlayıcı poz (bind pose) olarak da bilinir.
- **Bone (Kemik):** Sadece bir isim ve bir transform (pozisyon/rotasyon/ölçek) taşıyan mantıksal bir düğüm. Görsel bir karşılığı yok — mesh'i deforme etmek için kullanılan bir matematiksel referans noktası.
- **Pose (Poz):** Belirli bir andaki tüm bone'ların transform değerleri. Bir animasyon klibi, zamana yayılmış binlerce poz'un (aslında poz arasındaki anahtar karelerin/keyframe'lerin) dizisidir.

### A.2 Skeleton veri yapısı

```cpp
// Engine/Animation/Skeleton.h

struct Bone {
    std::string name;
    int parentIndex = -1;               // -1 = kök (root) bone
    Matrix4 bindPoseLocalTransform;     // Ebeveynine göre referans poz transformu
    Matrix4 inverseBindPoseWorldTransform; // ★ Skinning için şart — Bölüm C.2'de açıklanıyor
};

class Skeleton {
public:
    std::vector<Bone> bones;

    int findBoneIndex(const std::string& name) const {
        for (int i = 0; i < (int)bones.size(); i++)
            if (bones[i].name == name) return i;
        return -1;
    }

    // Bir bone'un ebeveyn zincirinden geçerek dünya-uzayı transformunu hesaplar
    std::vector<Matrix4> computeWorldTransforms(const std::vector<Matrix4>& localTransforms) const {
        std::vector<Matrix4> worldTransforms(bones.size());
        for (int i = 0; i < (int)bones.size(); i++) {
            worldTransforms[i] = (bones[i].parentIndex == -1)
                ? localTransforms[i]
                : worldTransforms[bones[i].parentIndex] * localTransforms[i]; // ★ Ebeveyn sıralı işlenmeli
        }
        return worldTransforms;
    }
};
```

**Önemli önkoşul:** `bones` dizisi, her zaman **ebeveyn çocuğundan önce** gelecek şekilde sıralı tutulmalı (topological order) — aksi halde `computeWorldTransforms` yanlış sonuç üretir çünkü `worldTransforms[bones[i].parentIndex]` henüz hesaplanmamış olabilir. Bu sıralama, asset import sırasında (Bölüm E) garanti ediliyor.

### A.3 AnimationClip veri yapısı

```cpp
// Engine/Animation/AnimationClip.h

struct BoneKeyframes {
    int boneIndex;
    std::vector<float> times;           // Anahtar karelerin zaman damgaları (saniye)
    std::vector<Vector3> positions;
    std::vector<Quaternion> rotations;
    std::vector<Vector3> scales;
};

class AnimationClip {
public:
    std::string name;
    float duration;
    bool looping = true;
    std::vector<BoneKeyframes> boneTracks;

    // Belirli bir zamanda, ilgili bone için interpolasyonlu (ara değer) transform hesaplar
    Matrix4 sampleBone(int boneIndex, float time) const {
        const BoneKeyframes* track = findTrack(boneIndex);
        if (!track) return Matrix4::identity(); // Bu bone bu klipte animasyonlu değil

        auto [prevIdx, nextIdx, t] = findSurroundingKeyframes(track->times, time);

        Vector3 pos = lerp(track->positions[prevIdx], track->positions[nextIdx], t);
        Quaternion rot = slerp(track->rotations[prevIdx], track->rotations[nextIdx], t); // ★ Rotasyonlar SLERP ile
        Vector3 scale = lerp(track->scales[prevIdx], track->scales[nextIdx], t);

        return Matrix4::fromTRS(pos, rot, scale);
    }
};
```

**Neden rotasyonlar `slerp` (spherical linear interpolation) ile, pozisyonlar `lerp` (linear interpolation) ile hesaplanıyor:** Rotasyonlar quaternion olarak temsil ediliyor ve quaternion'ların doğrusal (lerp) enterpolasyonu, sabit açısal hızla dönmeyen, "hızlanıp yavaşlayan" hatalı bir dönüş üretir. `slerp`, küresel yüzeyde sabit açısal hızla ilerleyen matematiksel olarak doğru enterpolasyonu sağlıyor. Bu, animasyon sistemlerinde asla atlanmaması gereken standart bir detaydır.

---

## Bölüm B — Blend Tree: Animasyonlar Arası Geçiş ve Karışım

### B.1 Neden ham "klip değiştirme" yetersiz

Karakter Kontrolcüsü dokümanının Bölüm C'sinde `playAnimation("Walk", blendTime=0.2f)` gibi bir çağrı görmüştük. Eğer bu, bir animasyondan diğerine **anlık** geçseydi (Idle pozundan aniden Walk pozuna atlama), görsel olarak rahatsız edici bir "sıçrama" (pop) oluşurdu. Çözüm, iki animasyon arasında zamana yayılı bir karışım (crossfade) yapmak.

### B.2 AnimationState ve geçiş (transition) sistemi

```cpp
// Engine/Animation/AnimationStateMachine.h

struct AnimationTransition {
    float blendDuration;
    float elapsedTime = 0.0f;
    AnimationClip* fromClip;
    AnimationClip* toClip;
    float fromClipTime, toClipTime;

    bool isComplete() const { return elapsedTime >= blendDuration; }
};

class AnimationPlayer {
public:
    void play(AnimationClip* clip, float blendDuration) {
        if (currentClip == clip) return; // Zaten bu klip çalıyor, tekrar başlatma

        if (currentClip) {
            activeTransition = AnimationTransition{blendDuration, 0.0f, currentClip, clip, currentTime, 0.0f};
        }
        currentClip = clip;
        currentTime = 0.0f;
    }

    // Her karede çağrılır — final pozu hesaplar
    std::vector<Matrix4> evaluate(const Skeleton& skeleton, float deltaTime) {
        currentTime += deltaTime;
        if (currentClip->looping) currentTime = std::fmod(currentTime, currentClip->duration);

        std::vector<Matrix4> finalPose(skeleton.bones.size());

        if (activeTransition && !activeTransition->isComplete()) {
            activeTransition->elapsedTime += deltaTime;
            float t = activeTransition->elapsedTime / activeTransition->blendDuration;

            for (int i = 0; i < (int)skeleton.bones.size(); i++) {
                Matrix4 fromPose = activeTransition->fromClip->sampleBone(i, activeTransition->fromClipTime);
                Matrix4 toPose = activeTransition->toClip->sampleBone(i, currentTime);
                finalPose[i] = blendTransforms(fromPose, toPose, t); // ★ İki poz arası karışım
            }
            if (activeTransition->isComplete()) activeTransition.reset();
        } else {
            for (int i = 0; i < (int)skeleton.bones.size(); i++)
                finalPose[i] = currentClip->sampleBone(i, currentTime);
        }
        return finalPose;
    }

private:
    AnimationClip* currentClip = nullptr;
    float currentTime = 0.0f;
    std::optional<AnimationTransition> activeTransition;
};
```

### B.3 Katmanlı animasyon (Layered Animation) — MVP sonrası genişleme notu

MVP'de tek bir `AnimationPlayer` (tüm vücut tek bir klip) yeterli. Ama ileride "koşarken aynı zamanda elle sallama" gibi senaryolar için **katmanlı** bir sistem gerekecek — örn. bel altı `Walk` klibinden, bel üstü `Wave` klibinden beslenip ikisi birleştirilir (bone mask ile). Bu, Faz 7'nin "sürekli iyileştirme" modeline benzer şekilde, MVP'nin ötesinde bir genişleme noktası olarak not ediliyor; bu dokümanın kapsamı dışında bırakılıyor.

---

## Bölüm C — Skinning: Animasyonun Mesh'e Uygulanması

### C.1 Skinned Mesh — normal mesh'ten farkı

Faz 2'deki `MeshHandle`, statik geometriyi temsil ediyordu. Bir karakterin mesh'i (skinned mesh) her vertex için ek veri taşır: **hangi bone'lardan ne kadar etkilendiği**.

```cpp
struct SkinnedVertex {
    Vector3 position;
    Vector3 normal;
    Vector2 uv;
    uint8_t boneIndices[4];  // Bu vertex'i etkileyen en fazla 4 bone
    float boneWeights[4];    // Her birinin etki ağırlığı (toplamları 1.0 olmalı)
};
```

**Neden 4 bone sınırı:** Bir dirsek bölgesindeki bir vertex, hem ön kol hem üst kol bone'undan aynı anda etkilenmeli (yumuşak deformasyon için) — ama pratikte 2-4 bone'un ağırlıklı ortalaması, gerçekçi bir deformasyon için yeterli. Bu, endüstri standardı bir sınırlama (Unreal, Unity de varsayılan olarak 4 kullanır); daha fazlası GPU'da gereksiz bant genişliği tüketir.

### C.2 Inverse Bind Pose — skinning'in matematiksel temeli

Bir vertex'in son (deforme olmuş) pozisyonu şu formülle hesaplanıyor:

```
finalPosition = Σ (boneWeight[i] * boneCurrentWorldTransform[i] * boneInverseBindPoseWorldTransform[i] * originalVertexPosition)
```

`inverseBindPoseWorldTransform`, bone'un **referans pozdaki** dünya transformunun tersidir. Bunun amacı: vertex, önce "bone'un referans pozundaki yerel uzayına" taşınıyor (ters transform ile), sonra bone'un **şu anki** (animasyonlu) dünya transformuyla tekrar dünya uzayına getiriliyor. Bu iki adımlı işlem, vertex'in bone'a göre "göreceli" pozisyonunu koruyarak, bone hareket ettiğinde vertex'in de doğru şekilde takip etmesini sağlıyor.

```cpp
// Bu matris, Skeleton import edilirken BİR KEZ hesaplanır, her karede değil
Matrix4 computeInverseBindPose(const Skeleton& skeleton, int boneIndex) {
    Matrix4 bindWorldTransform = /* bind pose'daki dünya transformu, ebeveyn zincirinden hesaplanır */;
    return bindWorldTransform.inverse();
}
```

### C.3 GPU Skinning — CPU'da değil, shader'da hesaplama

Yüzlerce vertex'i her karede CPU'da deforme etmek pahalıdır ve GPU'nun zaten çok iyi yaptığı bir paralel işi CPU'ya yüklemek anlamsızdır. Bu yüzden **bone matrisleri** GPU'ya bir uniform buffer olarak gönderilir, gerçek deformasyon vertex shader'da yapılır:

```glsl
// Engine/Renderer/Shaders/skinned_mesh.vertex.sc

uniform mat4 u_boneMatrices[MAX_BONES]; // Her bone için: currentWorldTransform * inverseBindPose (önceden çarpılmış)

void main() {
    mat4 skinMatrix =
        a_boneWeights[0] * u_boneMatrices[a_boneIndices[0]] +
        a_boneWeights[1] * u_boneMatrices[a_boneIndices[1]] +
        a_boneWeights[2] * u_boneMatrices[a_boneIndices[2]] +
        a_boneWeights[3] * u_boneMatrices[a_boneIndices[3]];

    vec4 skinnedPosition = skinMatrix * vec4(a_position, 1.0);
    vec3 skinnedNormal = mat3(skinMatrix) * a_normal; // Normal de aynı matrisle dönüştürülmeli (ışıklandırma doğru olsun diye)

    gl_Position = u_viewProj * u_model * skinnedPosition;
}
```

**Mimari akış:** Her karede CPU tarafında (Bölüm B'deki `AnimationPlayer::evaluate()`) hesaplanan `finalPose` (bone'ların o anki local transform'ları), `Skeleton::computeWorldTransforms()` ile dünya-uzayına çevrilir, `inverseBindPoseWorldTransform` ile çarpılır, ve sonuç `u_boneMatrices` uniform buffer'ına yazılıp bgfx üzerinden GPU'ya gönderilir. CPU sadece ~50-100 matrisi hesaplıyor (bone sayısı kadar); GPU ise binlerce vertex'i bu matrislerle paralel olarak deforme ediyor — iş, doğru işlemciye doğru şekilde dağıtılmış oluyor.

---

## Bölüm D — Inverse Kinematics (IK) — Ayakların Zemine Tam Oturması

### D.1 Neden gerekli

Bir yürüme animasyonu düz bir zeminde kaydedilmiştir. Karakter eğimli bir rampada veya düzensiz bir yüzeyde yürüdüğünde, animasyon klibi hâlâ "düz zemin" pozunu oynatır — bu da ayakların zeminin içine gömülmesi veya havada asılı kalması gibi görsel hatalara yol açar. IK, bu son-anki (post-animation) düzeltmeyi yapıyor.

### D.2 İki-Kemik IK (Two-Bone IK) — bacak/kol için yeterli, yaygın çözüm

Tam bir IK çözücü (genel amaçlı, N-kemikli zincirler için) karmaşık bir konudur (Jacobian tabanlı veya FABRIK algoritmaları). Ama bacak (uyluk-baldır-ayak) ve kol (üst kol-önkol-el) gibi **iki kemikli** zincirler için, trigonometriye dayalı basit ve verimli bir çözüm yeterli:

```cpp
// Engine/Animation/IK/TwoBoneIK.h

struct TwoBoneIKResult {
    Quaternion upperBoneRotation;
    Quaternion lowerBoneRotation;
};

TwoBoneIKResult solveTwoBoneIK(
    Vector3 rootPos, Vector3 midPos, Vector3 endPos,  // Animasyondan gelen mevcut poz (referans)
    Vector3 targetPos,                                  // Ayağın gerçekte olması gereken yer (zemin raycast sonucu)
    Vector3 poleVector                                  // Dizin/dirseğin hangi yöne bakacağını belirleyen referans nokta
) {
    float upperLength = (midPos - rootPos).length();
    float lowerLength = (endPos - midPos).length();
    float targetDistance = std::min((targetPos - rootPos).length(), upperLength + lowerLength - 0.01f); // Aşırı gerilmeyi önle

    // Kosinüs teoremi ile diz/dirsek açısı hesaplanır
    float cosAngle = (upperLength*upperLength + targetDistance*targetDistance - lowerLength*lowerLength)
                      / (2 * upperLength * targetDistance);
    float angle = std::acos(std::clamp(cosAngle, -1.0f, 1.0f));

    // ... poleVector kullanılarak dönüş düzlemi belirlenir, upperBoneRotation ve lowerBoneRotation hesaplanır
    return TwoBoneIKResult{ /* ... */ };
}
```

### D.3 IK'nın animasyon pipeline'ındaki yeri

```cpp
void CharacterAnimator::update(Humanoid& humanoid, float deltaTime) {
    std::vector<Matrix4> animatedPose = animationPlayer.evaluate(skeleton, deltaTime); // Bölüm B

    // ★ IK, animasyondan SONRA, skinning'den ÖNCE uygulanır
    if (ikEnabled) {
        Vector3 leftFootGroundHit = raycastForFoot(humanoid, LeftFoot, animatedPose);
        auto ikResult = solveTwoBoneIK(hipPos, kneePos, animatedPose[leftFootBoneIdx].getTranslation(),
                                         leftFootGroundHit, poleVector);
        animatedPose[leftUpperLegBoneIdx] = Matrix4::fromRotation(ikResult.upperBoneRotation) * /* ... */;
        animatedPose[leftLowerLegBoneIdx] = Matrix4::fromRotation(ikResult.lowerBoneRotation) * /* ... */;
    }

    skeleton.computeWorldTransforms(animatedPose); // Bölüm A.2 — sonra Bölüm C'deki skinning'e gönderilir
}
```

**Bu sıralama kritik:** IK, "animasyonun söylediği pozu düzelten" bir son-işlem (post-process) katmanı olarak tasarlanıyor. Bu sayede animasyon klipleri IK'dan tamamen habersiz kalabiliyor — aynı `Walk` klibi hem düz zeminde hem rampada kullanılabiliyor, farkı IK katmanı kapatıyor.

---

## Bölüm E — Asset Import: FBX'ten Motor Formatına

### E.1 Neden FBX işlemeyi sıfırdan yazmıyoruz

FBX, karmaşık, kısmen kapalı kaynaklı bir format. Bunu sıfırdan parse etmek haftalar/aylar sürer ve projenin "kimliğine" hiçbir katkı sağlamaz. Bu, net bir "hazır alınacaklar" kategorisi: **Assimp** (Open Asset Import Library) kütüphanesi FBX dahil onlarca formatı okuyup ortak bir ara veri yapısına çeviriyor.

```cpp
// Engine/Assets/Importers/SkeletalMeshImporter.cpp

#include <assimp/Importer.hpp>
#include <assimp/scene.h>

ImportedSkeletalMesh importFBX(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate | aiProcess_LimitBoneWeights | aiProcess_GenNormals);
    // aiProcess_LimitBoneWeights: ★ Assimp'e, vertex başına en fazla 4 bone ağırlığı bırakmasını,
    // fazlasını normalize ederek atmasını söylüyoruz (Bölüm C.1'deki sınırla tutarlı)

    Skeleton skeleton = extractSkeleton(scene);            // Bone hiyerarşisi + bind pose
    std::vector<AnimationClip> clips = extractAnimations(scene); // Her animasyon takımı ayrı bir klibe
    MeshData mesh = extractSkinnedMeshData(scene, skeleton); // Vertex + bone index/weight verisi

    // Faz 7'deki LOD zincirini de burada üretiyoruz — meshoptimizer'ın skinned mesh desteği kullanılır
    auto lodChain = generateLODChain(mesh); // Faz 7, Bölüm C.3 ile aynı fonksiyon, skinned veri de destekleniyor

    return {skeleton, clips, mesh, lodChain};
}
```

### E.2 Import sonrası — reflection'a bağlanma

```cpp
class AnimationTrack : public Instance {
public:
    std::shared_ptr<AnimationClip> clip;
    // Script'ten kontrol için:
    void play(float blendTime) { /* AnimationPlayer'ı tetikler */ }
    void stop(float fadeOutTime) { /* ... */ }
};

ClassBuilder<AnimationTrack>("AnimationTrack")
    .base("Instance")
    .method("Play", &AnimationTrack::play)
    .method("Stop", &AnimationTrack::stop);
```

Bu sayede bir Luau/C# scripti (Faz 3/8) `track:Play(0.2)` gibi bir çağrıyla animasyonu tetikleyebiliyor — tıpkı Roblox'taki `AnimationTrack` API'sinin karşılığı.

---

## Bölüm F — Networking ile Etkileşim: Animasyon Neden Ayrı Şekilde Senkronize Edilmeli

### F.1 Sorun: Her bone transformunu replicate etmek

Faz 6'daki genel replication sistemi (property bazlı) düşünülürse, ilk akla gelen "her bone'un transformunu bir property olarak replicate et" olurdu. Bu **kesinlikle yanlış** — bir karakterde 50-100 bone olabilir, her birini her karede ağa göndermek bant genişliğini anında tüketir.

### F.2 Doğru yaklaşım: Sadece "hangi animasyon çalıyor" senkronize edilir

```cpp
// Sunucu sadece şunu replicate eder:
struct AnimationStateReplication {
    InstanceId humanoidId;
    std::string currentClipName;  // "Walk", "Jump" vb.
    float clipStartServerTime;    // İstemci, bu zamandan bu yana ne kadar geçtiğini kendi hesaplar
    float blendDuration;
};
```

İstemci, bu küçük paketi aldığında **kendi tarafında** `AnimationPlayer::play("Walk", blendTime)` çağırır — asıl skeletal animasyon hesaplaması (Bölüm B, C) **her istemcide yerel olarak** çalışır. Bu, Faz 6'nın temel felsefesiyle (sadece "gerçeği" senkronize et, görsel detayı yerel hesapla) birebir örtüşüyor — render'ın DataModel'den beslenip yerel olarak hesaplanması gibi, animasyon da "hangi state" bilgisinden beslenip yerel olarak hesaplanıyor.

**Bu tasarımın bir sonucu:** Farklı istemcilerde aynı karakterin animasyonu, ağ gecikmesine bağlı olarak birkaç kare farkla senkronize olabilir — ama bu, oyuncuların fark edemeyeceği kadar küçük bir sapma, ve tam bone-seviyesi replication'ın maliyetinden çok daha ucuz bir çözüm.

---

## Bölüm G — "Definition of Done" Kontrol Listesi

- [ ] Assimp ile bir FBX dosyasından skeleton + en az bir animasyon klibi başarıyla import ediliyor
- [ ] `AnimationPlayer::evaluate()` doğru pozları hesaplıyor — rotasyonlarda `slerp` kullanıldığı doğrulanmış
- [ ] İki animasyon arasında blend/crossfade çalışıyor, geçiş sırasında görsel "sıçrama" olmuyor
- [ ] GPU skinning shader'ı çalışıyor — bone matrisleri doğru hesaplanıp uniform buffer'a yazılıyor, karakter mesh'i animasyona göre doğru deforme oluyor
- [ ] Vertex başına 4 bone sınırı ve ağırlık normalizasyonu doğru uygulanıyor (import sırasında Assimp ile)
- [ ] Two-Bone IK, eğimli bir rampada ayağın zemine doğru oturmasını sağlıyor (gömülme veya havada asılı kalma yok)
- [ ] IK, animasyon pozunu **sonradan düzelten** bir katman olarak doğru sırada uygulanıyor (animasyon → IK → skinning)
- [ ] Networking: animasyon state'i (klip adı + başlangıç zamanı) replicate ediliyor, **bone-seviyesi veri değil** — bant genişliği testiyle doğrulanmış
- [ ] Bir Luau/C# scriptinden `track:Play()` çağrısı doğru animasyonu tetikliyor
- [ ] LOD zinciri (Faz 7) skinned mesh'lerle de doğru çalışıyor — uzak mesafede basitleştirilmiş mesh, bone ağırlıklarını kaybetmeden render ediliyor

---

## Sonraki Adım Önerisi

Skeletal animation sistemiyle birlikte, karakterle ilgili görsel/gameplay zincirinin neredeyse tamamı (fizik, networking, animasyon, IK) tamamlanmış oldu. Kalan bekleyen konular:

1. **Asset Browser / import akışı:** Faz 4'ten beri bekliyor — artık FBX import da (Bölüm E) eklendiğine göre, bunu editörde görsel olarak yönetecek bir panel gerekiyor.
2. **Katmanlı animasyon (Layered Animation):** Bölüm B.3'te bilinçli olarak ertelenen konu.
3. **Interest Management derinleştirmesi:** Faz 6'dan beri bekliyor.

Hangisiyle devam edelim?
