# Faz 2 — Teknik Derinlemesine İnceleme
## Render Pipeline: DataModel'den Ekrana

Bu doküman, Faz 1'de kurulan DataModel/Reflection temelinin üzerine, Faz 2'nin (Render Pipeline) somut teknik kararlarını inceler. Ana soru şu: **`workspace` içindeki bir `Part`, her karede nasıl ekranda bir üçgen yığını olarak beliriyor?**

---

## Bölüm A — Kritik Mimari Karar: DataModel Ağacını Doğrudan Render Etmeyeceğiz

### A.1 Neden bu bir sorun?

İlk akla gelen naif yaklaşım şudur: Her karede `workspace`'in çocuklarını gez, her `Part` için bir çizim komutu gönder. Bu **çalışır ama ölçeklenmez.** Sebebi:

- `Instance` ağacı `shared_ptr` tabanlı, dallanmış bir yapı — bellekte dağınık (cache-unfriendly). Binlerce Part'ı her karede bu şekilde gezmek CPU'da gereksiz zaman kaybettirir.
- Render sistemi sadece "görünür, mesh'i olan" nesnelerle ilgilenir. Ama DataModel ağacında `Script`, `Sound`, `WeldConstraint` gibi görsel karşılığı olmayan yüzlerce nesne de var. Her karede bunları eleyerek gezmek gereksiz iş.
- Frustum culling (kamera görüş alanı dışındakileri eleme), sıralama (transparency, materyal bazlı batching) gibi işlemler **düz bir dizi (flat array)** üzerinde çok daha hızlı çalışır, ağaç yapısı üzerinde değil.

### A.2 Çözüm: Ayrı bir "Render Scene" — DataModel'den senkronize edilen düz bir liste

Roblox'un da, Unreal'ın da (Unreal'da buna kısmen "Scene Proxy" denir) kullandığı desen şudur: **Gameplay tarafı (DataModel, Instance ağacı) ile render tarafı (RenderScene, düz diziler) birbirinden ayrılır.** Aralarındaki köprü, property değiştiğinde tetiklenen bir senkronizasyon mekanizmasıdır.

```
┌─────────────────────┐         property değişince          ┌──────────────────────┐
│   DataModel (OOP)    │ ────── "dirty" işareti gönderir ───▶│   RenderScene (flat)  │
│                      │                                      │                       │
│  workspace           │                                      │  RenderProxy[] ────┐  │
│   └─ Part "Wall"     │                                      │  (transform,       │  │
│       .position      │                                      │   mesh handle,     │  │
│       .material      │                                      │   material handle) │  │
│   └─ Part "Floor"    │                                      │                     │  │
│       ...            │                                      └─────────────────────┘  │
└─────────────────────┘                                                                 │
                                                                     Renderer bunu │
                                                                     her kare okur ◀┘
```

Bu ayrımın somut anlamı: `Part` sınıfı, `position` property'si değiştiğinde (`setPosition()` çağrıldığında), doğrudan bgfx'e komut göndermez. Bunun yerine kendi `RenderProxy`'sini "dirty" (güncellenmesi gerekiyor) olarak işaretler. Renderer, her karenin başında sadece dirty olan proxy'lerin transform'unu günceller — geri kalanlara dokunmaz.

### A.3 RenderProxy tasarımı

```cpp
// Engine/Renderer/SceneGraph/RenderProxy.h

#pragma once
#include <cstdint>
#include "Engine/Core/Math/Matrix4.h"

using MeshHandle = uint32_t;      // Asset sisteminden gelen handle (Faz 2'de basit tutulur)
using MaterialHandle = uint32_t;

struct RenderProxy {
    Matrix4 worldTransform;
    MeshHandle mesh = InvalidHandle;
    MaterialHandle material = InvalidHandle;
    bool visible = true;
    bool castsShadow = true;

    // Culling için önceden hesaplanmış bounding sphere (her kare yeniden hesaplanmaz)
    Vector3 boundsCenter;
    float boundsRadius = 0.0f;
};

constexpr uint32_t InvalidHandle = 0xFFFFFFFF;
```

`RenderScene`, bu proxy'lerin **düz bir dizisini** (yani `std::vector<RenderProxy>`, ağaç değil) tutar:

```cpp
// Engine/Renderer/SceneGraph/RenderScene.h

class RenderScene {
public:
    // Instance ID -> proxy dizisindeki index eşlemesi
    uint32_t registerProxy(InstanceId ownerId, const RenderProxy& initial);
    void unregisterProxy(uint32_t proxyIndex);
    void markDirty(uint32_t proxyIndex, const Matrix4& newTransform);

    // Renderer bu fonksiyonu her karede çağırır
    const std::vector<RenderProxy>& getProxies() const { return proxies; }

private:
    std::vector<RenderProxy> proxies;                    // ★ Cache-friendly düz dizi
    std::unordered_map<InstanceId, uint32_t> ownerToIndex;
    std::vector<uint32_t> dirtyIndices;                   // Bu kare güncellenmesi gerekenler
};
```

### A.4 Part sınıfının RenderScene'e bağlanması

`Part`, `Instance`'tan miras aldığı için DataModel ağacında yaşıyor, ama **aynı zamanda** kendi `RenderProxy`'sine bir referans (index) tutuyor:

```cpp
class Part : public Instance {
public:
    void setPosition(const Vector3& newPos) {
        position = newPos;
        markRenderDirty(); // ★ Sadece proxy'yi dirty işaretler, hemen render etmez
    }

    void onAddedToWorkspace() override {
        // Part sahneye eklendiğinde bir RenderProxy talep eder
        RenderProxy proxy;
        proxy.mesh = getDefaultCubeMesh();
        proxy.material = resolveMaterialFromReflection();
        renderProxyIndex = RenderScene::instance().registerProxy(getInstanceId(), proxy);
    }

    void onRemovedFromWorkspace() override {
        RenderScene::instance().unregisterProxy(renderProxyIndex);
    }

private:
    void markRenderDirty() {
        if (renderProxyIndex != InvalidHandle) {
            Matrix4 transform = Matrix4::fromPositionAndSize(position, size);
            RenderScene::instance().markDirty(renderProxyIndex, transform);
        }
    }

    Vector3 position;
    Vector3 size{4.0f, 1.0f, 2.0f};
    uint32_t renderProxyIndex = InvalidHandle;
};
```

**Bu tasarımın kazandırdığı şey:** Gameplay kodu (`part.Position = ...`) hâlâ Roblox'taki kadar basit ve okunabilir kalıyor, ama arka planda render sistemi hiçbir zaman ağacı gezmiyor — sadece düz bir `RenderProxy` dizisi üzerinde çalışıyor. Bu, hem "kolay kullanım" hem "performans" hedefini aynı anda karşılıyor.

---

## Bölüm B — Forward mu Deferred mi? Somut Karar

### B.1 Seçenekler ve neden bu karar verildi

| Yaklaşım | Artı | Eksi | Roblox-benzeri hedefe uygunluk |
|---|---|---|---|
| **Klasik Forward** | Basit, transparency doğal çalışır | Çok ışık kaynağı olunca yavaşlar (her ışık her objede hesaplanır) | Basit ama sınırlı |
| **Deferred** | Binlerce ışık kaynağını verimli işler | Transparency (yarı saydam Part'lar) doğal desteklenmez, MSAA zor, bellek bant genişliği yüksek | ❌ Roblox'ta Transparency çok kullanılan bir property — deferred bunu zorlaştırır |
| **Clustered Forward (Forward+)** | Çok ışık desteği + transparency doğal çalışır + MSAA sorunsuz | Deferred'a göre biraz daha karmaşık kurulum | ✅ **Seçilen yaklaşım** |

**Karar: Clustered Forward Rendering.**

Gerekçe: Roblox tarzı sahnelerde oyuncular sürekli yarı saydam (`Transparency`) parçalar kullanır — neon tabelalar, cam, su efektleri. Deferred rendering'de transparent objeler G-Buffer'a yazılamadığı için ayrı bir forward-pass gerektirir, bu da mimariyi karmaşıklaştırır ("deferred + forward hibrit" gibi bir bakım yüküne yol açar). Clustered Forward ise transparency'yi baştan doğal olarak destekler ve yine de yüzlerce dinamik ışık kaynağını verimli işleyebilir — bu iki hedefi (hafiflik + görsel zenginlik) aynı anda karşılıyor.

### B.2 Clustered Forward'ın çalışma mantığı (özet)

```
1. Ekran, 3B bir ızgaraya (cluster) bölünür (örn. 16x9x24 hücre — X,Y ekranda, Z derinlikte)
2. Her karede, hangi ışığın hangi cluster'ları etkilediği hesaplanır (compute shader ile)
3. Her cluster'a "bu hücrede hangi ışıklar var" listesi yazılır (bir GPU buffer'a)
4. Objeler normal forward-shading ile çizilirken, hangi cluster'da olduklarına bakıp
   sadece o cluster'daki ışıkları hesaba katarlar (binlerce ışık yerine ~10-20 ışık/piksel)
```

Bu adım Faz 2'nin ileri kısmında (temel forward pipeline çalıştıktan sonra) eklenecek — **Faz 2'nin ilk hedefi klasik forward'ı çalıştırmak, clustering optimizasyonu ışık sayısı arttıkça (Faz 7 civarı) eklenecek bir katman.**

---

## Bölüm C — bgfx ile Somut Render Akışı

### C.1 View/Pass kavramı

bgfx'te her "geçiş" (örn. shadow pass, ana renk geçişi, post-process geçişi) bir `view id` ile tanımlanır:

```cpp
// Engine/Renderer/RenderPasses.h
enum RenderView : bgfx::ViewId {
    View_ShadowMap = 0,
    View_MainColor = 1,
    View_PostProcess = 2,
};
```

### C.2 Bir karenin render akışı (yüksek seviye)

```cpp
// Engine/Renderer/Renderer.cpp

void Renderer::renderFrame(const Camera& camera, const RenderScene& scene) {
    // 1. Dirty olan proxy'lerin transform'larını güncelle (Bölüm A.3)
    scene.flushDirtyTransforms();

    // 2. Frustum culling — sadece kamera görüş alanındaki proxy'ler işlenir
    std::vector<const RenderProxy*> visibleProxies = cullAgainstFrustum(scene.getProxies(), camera);

    // 3. Sıralama: Opak objeler önce (front-to-back, overdraw azaltmak için),
    //    transparent objeler sonra (back-to-front, doğru blending için)
    auto [opaque, transparent] = sortByMaterialAndDepth(visibleProxies, camera);

    // 4. Shadow pass — directional light'tan bakışla derinlik haritası oluştur
    renderShadowPass(View_ShadowMap, scene.getDirectionalLight());

    // 5. Ana renk geçişi
    bgfx::setViewTransform(View_MainColor, camera.viewMatrix, camera.projMatrix);
    for (auto* proxy : opaque)      submitDrawCall(View_MainColor, proxy);
    for (auto* proxy : transparent) submitDrawCall(View_MainColor, proxy, /*blend=*/true);

    bgfx::frame(); // GPU'ya gönder
}
```

### C.3 Materyal sistemi — PBR temel uniform yapısı

Faz 2'de hedeflenen, endüstri standardı Metallic/Roughness PBR modeli:

```cpp
// Engine/Renderer/Materials/Material.h

struct MaterialData {
    Vector3 albedo{1,1,1};
    float metallic = 0.0f;
    float roughness = 0.5f;
    float emissiveStrength = 0.0f;
    TextureHandle albedoTexture = InvalidHandle;
    TextureHandle normalTexture = InvalidHandle;
};
```

Shader tarafı (bgfx `.sc` shader dili, HLSL benzeri):

```glsl
// Engine/Renderer/Shaders/pbr_forward.fragment.sc

uniform vec4 u_albedoRoughness;   // xyz: albedo, w: roughness
uniform vec4 u_metallicEmissive;  // x: metallic, y: emissive

void main() {
    vec3 albedo = u_albedoRoughness.xyz;
    float roughness = u_albedoRoughness.w;
    float metallic = u_metallicEmissive.x;

    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_cameraPos - v_worldPos);

    vec3 result = vec3(0.0);
    for (int i = 0; i < u_lightCount; i++) {
        result += computeCookTorranceBRDF(N, V, lights[i], albedo, metallic, roughness);
    }

    gl_FragColor = vec4(result, 1.0);
}
```

**Not:** `computeCookTorranceBRDF` fonksiyonunun tam implementasyonu (Cook-Torrance/GGX dağılım modeli) standart bir PBR formülüdür — bgfx'in örnek projelerinde (`bgfx/examples/13-stencil` ve PBR örnekleri) referans implementasyon mevcuttur, sıfırdan türetmeye gerek yok.

---

## Bölüm D — Kamera Sistemi

```cpp
// Engine/Renderer/Camera.h

class Camera {
public:
    Vector3 position{0, 5, -10};
    Vector3 forward{0, 0, 1};
    Vector3 up{0, 1, 0};
    float fovDegrees = 70.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    Matrix4 getViewMatrix() const {
        return Matrix4::lookAt(position, position + forward, up);
    }

    Matrix4 getProjectionMatrix(float aspectRatio) const {
        return Matrix4::perspective(fovDegrees, aspectRatio, nearPlane, farPlane);
    }
};
```

Faz 2'nin MVP'si için tek bir "free-fly" kamera yeterli (editördeki gezinme kamerası). Oyun-içi kamera davranışları (üçüncü şahıs takip, vb.) Faz 4/Scripting entegrasyonu sonrasına bırakılıyor çünkü bu davranışlar Luau scriptleriyle kontrol edilecek.

---

## Bölüm E — Gölgeleme: Basitten Başlama Stratejisi

### E.1 Faz 2'de hedeflenen: Tek Cascade Shadow Map

Gerçekçi motorlarda (Unreal, Unity) kullanılan **CSM (Cascaded Shadow Maps)** — sahneyi kameraya olan uzaklığa göre birden fazla gölge haritasına bölme — teknik olarak karmaşık ve Faz 2'nin MVP hedefini geciktirir. Bu yüzden:

**Faz 2'de:** Tek bir shadow map (directional light için), sabit bir mesafeye kadar (örn. 50 birim) gölge.
**Faz 7'de:** Çoklu cascade'e genişletme (uzak/yakın mesafelerde farklı çözünürlük).

```cpp
void Renderer::renderShadowPass(bgfx::ViewId view, const DirectionalLight& light) {
    Matrix4 lightView = Matrix4::lookAt(light.position, light.position + light.direction, {0,1,0});
    Matrix4 lightProj = Matrix4::orthographic(-30, 30, -30, 30, 0.1f, 100.0f);

    bgfx::setViewTransform(view, lightView, lightProj);
    bgfx::setViewFrameBuffer(view, shadowMapFrameBuffer);

    for (auto* proxy : sceneOpaqueProxies) {
        if (proxy->castsShadow) submitDepthOnlyDrawCall(view, proxy);
    }
}
```

Ana renk geçişinde bu shadow map bir texture olarak örneklenir ve piksel gölgede mi değil mi kontrol edilir (standart shadow mapping tekniği — PCF/percentage-closer filtering ile kenar yumuşatma önerilir).

---

## Bölüm F — Faz 2 "Definition of Done" Kontrol Listesi

- [ ] `RenderScene` ve `RenderProxy` sistemleri çalışıyor — bir `Part` oluşturulup pozisyonu değiştirildiğinde, `markDirty` doğru tetikleniyor
- [ ] DataModel ağacı her karede **gezilmiyor** — bunun yerine sadece dirty proxy'ler güncelleniyor (performans testiyle doğrulanmalı: 10.000 Part sahnesinde FPS düşüşü kabul edilebilir seviyede)
- [ ] Free-fly kamera çalışıyor (WASD + mouse look)
- [ ] Frustum culling aktif — kamera arkasındaki/dışındaki objeler çizilmiyor (debug görselleştirmeyle doğrulanmalı)
- [ ] Temel PBR materyal sistemi çalışıyor (albedo, metallic, roughness parametreleri görsel olarak fark yaratıyor)
- [ ] En az bir directional light + tek cascade shadow map çalışıyor
- [ ] Transparency (Part.Transparency property'si) doğru blend ile render ediliyor, opak objelerin arkasında/önünde doğru sıralanıyor
- [ ] Opak/transparent sıralama doğru çalışıyor (bir transparent Part, arkasındaki opak Part'ı doğru gösteriyor)

---

## Sonraki Adım Önerisi

Render pipeline'ın temeli kurulduktan sonra doğal sıradaki adım **Faz 3 — Luau Scripting Entegrasyonu**: Bölüm B'de (önceki dokümanda) tasarlanan reflection sisteminin, Luau VM'ine nasıl bağlanacağı — yani `part.Position = Vector3.new(0,10,0)` satırının C++ tarafında adım adım hangi fonksiyonları tetiklediği.

Faz 3'e mi geçelim, yoksa Faz 2'nin başka bir alt başlığını (örneğin asset/mesh import sistemi, ya da post-processing pipeline) mı derinleştirelim?

