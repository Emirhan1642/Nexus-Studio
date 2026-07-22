# Karakter Kontrolcüsü — Teknik Derinlemesine İnceleme
## Humanoid Sistemi: Fizik, Animasyon ve Networking'in Kesişimi

Bu doküman, Faz 5 (Fizik), Faz 6 (Networking) ve Faz 3/8'de (Scripting) kurulan sistemlerin hepsini bir araya getiren, projenin en karmaşık gameplay bileşenini inceler: Roblox'taki `Humanoid` benzeri bir karakter kontrolcüsü. Hedef: `humanoid.WalkSpeed = 16` gibi bir script satırının, bir oyuncunun ekranda pürüzsüzce yürümesine dönüşmesi.

**Neden bu kadar karmaşık:** Karakter kontrolcüsü, motorun neredeyse her sistemine dokunan tek gameplay bileşenidir — fizik (çarpışma/yerçekimi), animasyon (yürüme/koşma), networking (prediction/reconciliation), scripting (WalkSpeed gibi property'ler) ve reflection (hepsinin birbirine bağlanması) burada kesişiyor.

---

## Bölüm A — Temel Mimari Karar: Neden Normal Rigid Body Yeterli Değil

### A.1 Sorun: Fiziksel olarak "doğru" ama oynanabilir olarak "yanlış"

Faz 5'te bir `Part`'ı Jolt'un normal rigid body sistemiyle simüle ettik — yerçekimi, sürtünme, çarpışma tepkisi (impulse-based). Bir karakteri de aynı şekilde bir rigid body olarak simüle etmeye çalışırsak ciddi sorunlar çıkar:

- **Merdivenler/basamaklar:** Gerçek fizik, küçük bir basamağa çarpan bir küpü ya durdurur ya da devirir. Oyuncular basamaklarda takılmadan yürümeyi bekler.
- **Eğimli yüzeyler:** Bir rigid body eğimli bir rampada yerçekimiyle kayar. Bir karakterin rampada durabilmesi, hatta rampayı tırmanabilmesi gerekir.
- **Ani yön değişimi:** Gerçek fizikte bir cismin yönünü aniden değiştirmek için büyük kuvvetler gerekir (ataletin doğal sonucu). Oyuncular ise karakterin tuşa basar basmaz anında yön değiştirmesini bekler.

**Çözüm: Jolt'un `CharacterVirtual` sınıfı** — bu, Jolt'un tam fizik simülasyonundan **ayrı**, özel olarak karakter hareketi için tasarlanmış bir sistem. Fiziksel olarak "gerçekçi" olmak yerine "oynanabilir" olmaya odaklanıyor; bu ayrım hemen hemen tüm modern oyun motorlarında (Unreal'ın `CharacterMovementComponent`'i, Unity'nin `CharacterController`'ı) aynı şekilde yapılıyor.

### A.2 CharacterVirtual'ın çalışma mantığı (özet)

```
Normal rigid body:  Kuvvet uygula → Jolt solver'ı çöz → sonucu oku (dolaylı kontrol)
CharacterVirtual:    "Bu yöne, bu hızda hareket etmek istiyorum" → sistem engelleri
                      (basamak, eğim, duvar) hesaba katarak SEN yerine pozisyonu hesaplar (doğrudan kontrol)
```

`CharacterVirtual`, her karede "shape cast" (karakterin capsule/cylinder şeklini istenen yöne doğru sanal olarak kaydırıp neyle çarpıştığını bulma) tekniğini kullanarak basamak çıkma, eğim sınırlama gibi davranışları dahili olarak çözüyor — bunları sıfırdan yazmamıza gerek yok, tam olarak "hazır alınacaklar" kategorisine giriyor.

---

## Bölüm B — Humanoid Sınıfının Reflection Kaydı

### B.1 Temel yapı

```cpp
// Engine/Core/DataModel/Humanoid.h

#include <Jolt/Physics/Character/CharacterVirtual.h>

class Humanoid : public Instance {
public:
    float walkSpeed = 16.0f;      // stud/saniye
    float jumpPower = 50.0f;
    float maxHealth = 100.0f;
    float health = 100.0f;
    HumanoidState state = HumanoidState::Idle;

    void moveTo(const Vector3& direction); // Script'ten çağrılır — pathfinding değil, doğrudan yön
    void jump();

private:
    JPH::Ref<JPH::CharacterVirtual> character;
    std::weak_ptr<Instance> rootPart; // Karakterin fiziksel gövdesini temsil eden Part
};

enum class HumanoidState { Idle, Walking, Jumping, Falling, Landed, Climbing, Ragdoll };
```

### B.2 Reflection kaydı — önceki fazlarla tutarlılık

```cpp
ClassBuilder<Humanoid>("Humanoid")
    .base("Instance")
    .property("WalkSpeed", &Humanoid::walkSpeed).category("Movement")
    .property("JumpPower", &Humanoid::jumpPower).category("Movement")
    .property("Health", &Humanoid::health).category("Stats")
    .property("MaxHealth", &Humanoid::maxHealth).category("Stats")
    .enumProperty("State", &Humanoid::state, "HumanoidState").category("Movement")
    .method("MoveTo", &Humanoid::moveTo)
    .method("Jump", &Humanoid::jump);
```

**Bu kayıt sayesinde:** `humanoid.WalkSpeed = 24` bir script'ten (Luau veya C#, Faz 8'deki ikisi de aynı zincire bağlı) çalıştığında, doğrudan `CharacterVirtual`'ın hareket hesaplamasında kullanılan değeri güncelliyor — hiçbir özel binding kodu gerekmiyor, çünkü Faz 1'den beri kurduğumuz mekanizma zaten bunu otomatik hallediyor.

---

## Bölüm C — Hareket Durum Makinesi (State Machine)

### C.1 Neden bir state machine gerekiyor

Karakterin davranışı ("yürüyor mu, düşüyor mu, zıplıyor mu") hem animasyonun hangi klibi oynatacağını hem de bazı fizik kararlarını (örn. havadayken yön değiştirme hassasiyeti düşük olmalı) belirliyor. Bu, düzenli bir state machine ile yönetilmeli — dağınık `if` zincirleri yerine.

```cpp
// Engine/Physics/Character/HumanoidStateMachine.h

class HumanoidStateMachine {
public:
    void update(Humanoid& humanoid, float deltaTime) {
        HumanoidState newState = computeNextState(humanoid);

        if (newState != humanoid.state) {
            onStateExit(humanoid.state, humanoid);
            onStateEnter(newState, humanoid);
            humanoid.state = newState; // ★ Reflection setter üzerinden değil, doğrudan — state her karede değişebilir, Undo/replication'ı her seferinde tetiklemek istemeyiz (Bölüm F'de bu ayrım detaylandırılıyor)
        }
    }

private:
    HumanoidState computeNextState(const Humanoid& humanoid) {
        JPH::CharacterVirtual* character = humanoid.getCharacter();
        JPH::CharacterVirtual::EGroundState groundState = character->GetGroundState();

        if (groundState == JPH::CharacterVirtual::EGroundState::InAir) {
            return character->GetLinearVelocity().GetY() > 0
                ? HumanoidState::Jumping
                : HumanoidState::Falling;
        }
        if (humanoid.state == HumanoidState::Jumping || humanoid.state == HumanoidState::Falling) {
            return HumanoidState::Landed; // Bir karelik geçiş durumu — animasyon/ses tetiklemek için
        }

        bool hasMoveInput = humanoid.getCurrentMoveDirection().length() > 0.01f;
        return hasMoveInput ? HumanoidState::Walking : HumanoidState::Idle;
    }

    void onStateEnter(HumanoidState state, Humanoid& humanoid) {
        // Roblox'taki humanoid.StateChanged event'i burada tetiklenir (Faz 3'teki Signal sistemi)
        humanoid.stateChangedSignal.fire({state});
    }
};
```

### C.2 Hareket uygulaması — CharacterVirtual'a komut verme

```cpp
void HumanoidController::applyMovement(Humanoid& humanoid, const Vector3& moveDirection, float deltaTime) {
    JPH::CharacterVirtual* character = humanoid.getCharacter();

    JPH::Vec3 desiredVelocity = toJoltVec3(moveDirection) * humanoid.walkSpeed;
    desiredVelocity.SetY(character->GetLinearVelocity().GetY()); // Yatay hız kontrolümüz, dikey hızı yerçekimi belirliyor

    character->SetLinearVelocity(desiredVelocity);

    // ExtendedUpdate: basamak çıkma, eğim sınırlama, yapışkan zemin (sticky ground) hesaplamalarını içeren
    // Jolt'un hazır sunduğu üst-seviye güncelleme fonksiyonu
    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.4f, 0); // ~0.4 stud'a kadar basamak çıkabilir

    character->ExtendedUpdate(deltaTime, toJoltVec3({0, -gravity, 0}), updateSettings,
                                broadPhaseLayerFilter, objectLayerFilter, bodyFilter, shapeFilter, tempAllocator);

    // ★ Faz 5, Bölüm C.3'teki senkronizasyon deseninin aynısı — Jolt sonucu DataModel'e geri yazılıyor
    auto part = humanoid.getRootPart();
    part->position = fromJoltVec3(character->GetPosition());
    part->markRenderDirty();
}
```

---

## Bölüm D — Networking Entegrasyonu: Humanoid için Client-Side Prediction

### D.1 Faz 6'daki genel prediction sisteminin somutlaşması

Faz 6, Bölüm D'de genel bir `ClientPredictor` tasarlamıştık. Humanoid, bu sistemin **gerçek, somut kullanım alanı**:

```cpp
// Engine/Networking/Prediction/HumanoidPredictor.h

struct HumanoidInputCommand : InputCommand { // Faz 6, Bölüm D.2'deki temel struct'tan türetilir
    Vector3 moveDirection;
    bool jumpRequested;
};

class HumanoidPredictor {
public:
    void onLocalInput(const HumanoidInputCommand& cmd) {
        pendingCommands.push_back(cmd);

        // ★ İstemci, sunucuyu beklemeden HEMEN CharacterVirtual'ı ilerletir
        HumanoidController::applyMovement(*localHumanoid, cmd.moveDirection, cmd.deltaTime);
        if (cmd.jumpRequested) localHumanoid->jump();

        sendToServer(cmd);
    }

    void onServerSnapshot(uint32_t ackedSeq, const Vector3& serverPos, const Vector3& serverVelocity) {
        pendingCommands.erase(/* onaylanan komutları temizle — Faz 6, Bölüm D.2 ile birebir aynı desen */);

        localHumanoid->getCharacter()->SetPosition(toJoltVec3(serverPos));
        localHumanoid->getCharacter()->SetLinearVelocity(toJoltVec3(serverVelocity));

        // Onaylanmamış komutları TEKRAR uygula (replay)
        for (auto& cmd : pendingCommands)
            HumanoidController::applyMovement(*localHumanoid, cmd.moveDirection, cmd.deltaTime);
    }
};
```

### D.2 Neden Humanoid, prediction'ın "zor" örneği

Faz 6'da bahsedilmeyen bir zorluk burada ortaya çıkıyor: **basamak çıkma ve eğim hesaplamaları deterministik olmayabilir.** Jolt'un `ExtendedUpdate` fonksiyonu, dünyadaki diğer objelerle (örn. hareketli bir platform) etkileşime girdiğinde, istemci ve sunucunun aynı "dünya görüşüne" (world state) sahip olmaması durumunda farklı sonuçlar üretebilir. Bu, Faz 6, Bölüm C.4'teki relevancy sisteminin bir yan etkisiyle birleşiyor: eğer istemci, karakterin bastığı platformun güncel pozisyonunu henüz almadıysa, prediction hesaplaması sunucununkinden sapabilir.

**Pratik önlem:** Hareketli platformlar gibi "karakterin hareketini etkileyebilecek" objeler, relevancy filtresinden **bağımsız olarak her zaman yüksek öncelikli** replicate edilmeli (Faz 6, Bölüm C.4'teki `isRelevant()` fonksiyonuna bir istisna kategorisi eklenir). Bu, ileride Interest Management derinleştirilirken ayrıca ele alınması gereken bir detay.

---

## Bölüm E — Animasyon Sistemi ile Bağlantı (Temel Düzey)

### E.1 Faz 7'nin kapsamı dışında bırakılan konu: Skeletal Animation

Bu dokümanın kapsamı, tam bir skeletal animation sisteminin (bone hiyerarşisi, blend tree, IK) tasarımını içermiyor — bu, ayrı bir derinleştirme dokümanı gerektirecek kadar büyük bir konu. Ama Humanoid'in state machine'i ile animasyon sisteminin **arayüzünü** tanımlamak, ileride bu sistemi eklerken Humanoid tarafında değişiklik gerekmemesi için önemli:

```cpp
// Engine/Animation/AnimationController.h (gelecekteki tam implementasyonun iskeleti)

class AnimationController {
public:
    // HumanoidStateMachine, her state değişiminde bunu çağırır — animasyon sisteminin
    // detayları (blend süresi, hangi klip) Humanoid'den tamamen izole
    void onHumanoidStateChanged(HumanoidState newState) {
        switch (newState) {
            case HumanoidState::Walking: playAnimation("Walk", /*blendTime=*/0.2f); break;
            case HumanoidState::Jumping: playAnimation("Jump", /*blendTime=*/0.1f); break;
            case HumanoidState::Falling: playAnimation("Fall", /*blendTime=*/0.3f); break;
            case HumanoidState::Idle:    playAnimation("Idle", /*blendTime=*/0.25f); break;
        }
    }
};
```

`HumanoidStateMachine::onStateEnter` (Bölüm C.1), state değişimini `stateChangedSignal` ile yayınladığı için, `AnimationController` bu sinyale abone olarak çalışır — Humanoid, animasyon sisteminin var olup olmadığını bilmez. Bu, projedeki tanıdık desenin (event-driven, gevşek bağlı sistemler) bir kez daha tekrarı.

---

## Bölüm F — Ragdoll: Fizik Moduna Geçiş

### F.1 Neden bu ayrı bir zorluk

Bir karakter öldüğünde veya sersemlediğinde (Roblox'taki "ragdoll" efekti), `CharacterVirtual`'ın "oynanabilir ama gerçekçi değil" modelinden, Faz 5'teki **gerçek rigid body fiziğine** geçmesi gerekiyor — karakterin her uzvu (kol, bacak, gövde) artık gerçek yerçekimi ve çarpışmayla düşüp yuvarlanmalı.

```cpp
void Humanoid::enterRagdoll() {
    character->SetEnabled(false); // CharacterVirtual devre dışı

    // Her uzuv için önceden tanımlı bir rigid body + constraint zinciri aktive edilir
    for (auto& limb : ragdollLimbs) {
        JPH::BodyCreationSettings settings(limb.shape, limb.currentTransform, /*...*/,
                                             JPH::EMotionType::Dynamic, Layers::RAGDOLL);
        limb.bodyId = physicsWorld.getBodyInterface().CreateAndAddBody(settings, JPH::EActivation::Activate);
    }
    for (auto& jointConstraint : ragdollJoints) {
        physicsWorld.addConstraint(jointConstraint); // Omuz, dirsek, diz gibi eklemler arası kısıtlamalar
    }

    state = HumanoidState::Ragdoll;
    stateChangedSignal.fire({HumanoidState::Ragdoll});
}
```

**Bu geçişin önemi:** Sistem, aynı `Instance` (karakterin görsel/reflection temsili) üzerinde iki farklı fizik temsilinin (CharacterVirtual ve rigid body zinciri) **birbirini dışlayan** şekilde çalışmasını yönetiyor — herhangi bir anda ikisinden sadece biri aktif. Bu, Faz 5'teki `PhysicsBodyHandle` desenine ek bir katman olarak, "hangi fizik modunun aktif olduğu" bilgisini de taşıması gerektiği anlamına geliyor.

---

## Bölüm G — Kamera Entegrasyonu (Kısa Not)

Faz 2, Bölüm D'de tanımlanan `Camera` sınıfı, Faz 4'e kadar sadece editörün free-fly kamerası için kullanılmıştı. Humanoid ile birlikte, oyun-içi bir üçüncü şahıs takip kamerası ihtiyacı doğuyor:

```cpp
class HumanoidFollowCamera {
public:
    void update(Camera& camera, const Humanoid& humanoid, float deltaTime) {
        Vector3 targetPos = humanoid.getRootPart()->position + Vector3(0, cameraHeight, 0);
        Vector3 desiredCameraPos = targetPos - camera.forward * followDistance;

        // Yumuşak takip (smoothing) — kamera aniden zıplamasın
        camera.position = lerp(camera.position, desiredCameraPos, 1.0f - std::exp(-cameraSmoothing * deltaTime));

        // Duvara çarpma kontrolü — kamera bir duvarın arkasına geçmemeli
        auto hit = physicsWorld.raycast(targetPos, camera.position);
        if (hit) camera.position = hit.point;
    }
};
```

Bu, Faz 3'teki Luau binding'i üzerinden script'e de açılabilir (`humanoid.CameraOffset` gibi bir property ile) — ama bu dokümanın kapsamı, kameranın temel takip mekaniğiyle sınırlı tutuluyor; tam bir kamera sistemi (birinci şahıs, sabit açı modları vb.) ayrı bir derinleştirme konusu olabilir.

---

## Bölüm H — "Definition of Done" Kontrol Listesi

- [ ] `CharacterVirtual` entegre edilmiş, karakter düz zeminde yürüyebiliyor
- [ ] Basamak çıkma (~0.4 stud'a kadar) ve orta eğimli rampalarda yürüme sorunsuz çalışıyor
- [ ] `humanoid.WalkSpeed` ve `JumpPower` script'ten değiştirildiğinde hemen etkili oluyor (Luau ve C#'ın ikisinde de test edilmiş)
- [ ] State machine doğru çalışıyor — Idle/Walking/Jumping/Falling/Landed geçişleri doğru tetikleniyor, `StateChanged` sinyali doğru ateşleniyor
- [ ] İstemci tarafı prediction çalışıyor — yapay gecikme altında karakter hareketi anlık hissediliyor (Faz 6'daki genel testin Humanoid'e özel tekrarı)
- [ ] Hareketli bir platform üzerinde duran karakterin prediction'ı, platform relevancy istisnası sayesinde sunucuyla senkron kalıyor
- [ ] Ragdoll geçişi çalışıyor — `CharacterVirtual` devre dışı kalıp uzuvlar gerçek fizikle simüle ediliyor, geçiş sırasında karakter "zıplama" veya "içine gömülme" gibi görsel bir hata sergilemiyor
- [ ] Takip kamerası, karakter hareket ederken pürüzsüz takip ediyor ve duvarların arkasına geçmiyor
- [ ] `AnimationController` (iskelet düzeyinde) state değişimlerine doğru abone oluyor — Humanoid, animasyon sisteminin varlığından habersiz kalmaya devam ediyor (bağımlılık yönü doğru)

---

## Sonraki Adım Önerisi

Karakter kontrolcüsü ile birlikte proje artık "oynanabilir" bir noktaya geldi. Bekleyen konular:

1. **Tam Skeletal Animation sistemi:** Bölüm E'de bilinçli olarak kapsam dışı bırakılan bone hiyerarşisi, blend tree, IK — kendi başına büyük bir derinleştirme konusu.
2. **Asset Browser / import akışı:** Faz 4'ten beri bekliyor.
3. **Interest Management derinleştirmesi:** Faz 6'dan beri bekliyor, artık hareketli platform senaryosuyla (Bölüm D.2) bir bağımlılığı da ortaya çıktı.

Hangisiyle devam edelim?
