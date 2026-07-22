# Faz 7 — Teknik Derinlemesine İnceleme
## Gelişmiş Grafik Kalitesi: GI, Post-Processing, LOD

Bu doküman, Faz 2'de kurulan temel Clustered Forward render pipeline'ının üzerine, "Unreal kadar gerçekçi" hedefine yaklaştıracak gelişmiş tekniklerin nasıl ekleneceğini inceler. Faz 2'de bilinçli olarak ertelenen konular (gerçek GI, çoklu cascade gölgeler, LOD) burada ele alınıyor.

**Önemli çerçeveleme:** Bu faz, Faz 2'nin aksine "bitmiş" bir hedefi yok — sürekli iyileştirilebilecek bir alandır. Doküman, MVP kalitesinde başlangıç noktaları sunuyor; her biri daha sonra ayrı ayrı derinleştirilebilir.

---

## Bölüm A — Global Illumination: Hangi Teknik, Neden

### A.1 Seçenekler ve gerçekçi değerlendirme

| Teknik | Kalite | Performans Maliyeti | Uygulama Zorluğu | Roblox-tarzı dinamik sahnelere uygunluk |
|---|---|---|---|---|
| Lightmap baking (statik) | Yüksek ama sadece statik objelerde | Çok düşük (runtime'da bedava) | Orta | ❌ Kullanıcılar sahneyi sürekli değiştiriyor, her değişiklikte yeniden bake gerekir |
| Voxel Cone Tracing (VCT) | Orta-yüksek, dinamik | Yüksek | Yüksek | ✅ Dinamik sahnelerde makul |
| Screen-Space GI (SSGI) | Düşük-orta, artefaktlı | Düşük-orta | Düşük-orta | ⚠️ Ekranda görünmeyen yüzeylerde ışık kaybolur |
| Ray-traced GI (Lumen tarzı) | Çok yüksek | Çok yüksek, özel donanım gerekir | Çok yüksek | ❌ "Hafif" hedefiyle çelişir |

**Karar: Voxel Cone Tracing (VCT), SSGI ile desteklenmiş hibrit yaklaşım.**

Gerekçe: Roblox tarzı bir motorun temel özelliği kullanıcıların sahneyi **sürekli, gerçek zamanlı** değiştirmesi — bir Part eklendiğinde/taşındığında ışıklandırmanın yeniden hesaplanması (re-bake) saniyeler sürerse editör deneyimi bozulur. Lightmap baking bu yüzden elendi. Tam ray-tracing (Lumen'in kullandığı teknik) ise "hafif motor" hedefiyle doğrudan çelişiyor — düşük/orta seviye donanımda çalışmayacak. VCT, dinamik sahnelerde makul kalite/performans dengesi sunan, orta seviye GPU'larda da çalışabilen bir orta yol.

### A.2 VCT'nin temel çalışma mantığı (kavramsal özet)

```
1. Sahne, düzenli aralıklarla bir "voxel grid"e (3B küp ızgara) dönüştürülür
   (her voxel, o bölgedeki yüzeylerin albedo + normal + emissive bilgisini taşır)
2. Bu voxel grid'in her seviyesi için bir mipmap zinciri oluşturulur
   (uzak mesafeler için kaba, yakın mesafeler için detaylı voxel verisi)
3. Her pikselden, yüzey normali etrafında birkaç "koni" (cone) fırlatılır
4. Her koni, voxel mipmap zincirinde ilerleyerek dolaylı ışığı toplar
5. Toplanan ışık, doğrudan aydınlatmaya (Faz 2'deki Cook-Torrance BRDF) eklenir
```

### A.3 Voxelization pass

```cpp
// Engine/Renderer/GI/Voxelizer.h

class Voxelizer {
public:
    void voxelizeScene(const std::vector<RenderProxy*>& proxies) {
        bgfx::setViewFrameBuffer(View_Voxelize, voxelTarget);
        bgfx::setViewRect(View_Voxelize, 0, 0, VOXEL_RESOLUTION, VOXEL_RESOLUTION);

        // Sahne, voxel grid'in kapladığı hacme ortografik olarak 3 eksenden projekte edilir
        // (conservative rasterization ile ince geometrilerin voxel'i kaçırmaması sağlanır)
        for (auto* proxy : proxies) {
            submitVoxelizationDrawCall(View_Voxelize, proxy, voxelizationShader);
        }

        generateVoxelMipmaps(); // ★ Cone tracing'in farklı mesafelerde çalışabilmesi için şart
    }

private:
    static constexpr int VOXEL_RESOLUTION = 256; // Her eksende 256 voxel — bellek/kalite dengesi
    bgfx::FrameBuffer voxelTarget; // 3D texture (Texture3D) olarak tutulur
};
```

**Önemli mühendislik notu:** Tüm sahneyi her karede yeniden voxelize etmek pahalıdır. Pratik bir optimizasyon: **sadece değişen bölgeleri** yeniden voxelize etmek (Faz 2'deki "dirty" deseninin burada da bir varyasyonu) — bir Part hareket ettiğinde sadece onun bulunduğu voxel bölgesi güncellenir, statik sahnenin geri kalanı dokunulmadan kalır.

### A.4 Cone tracing (ışıklandırma sırasında, ana shader'da)

```glsl
// Engine/Renderer/Shaders/pbr_forward.fragment.sc içine eklenen GI katkısı

vec3 traceCone(vec3 origin, vec3 direction, float coneAperture) {
    vec3 accumulated = vec3(0.0);
    float distance = VOXEL_SIZE;

    while (distance < MAX_TRACE_DISTANCE && accumulated.a < 1.0) {
        float diameter = 2.0 * distance * tan(coneAperture * 0.5);
        float mipLevel = log2(diameter / VOXEL_SIZE); // Uzaklaştıkça daha kaba mip seviyesi

        vec4 voxelSample = textureLod(u_voxelTexture, worldToVoxelUV(origin + direction * distance), mipLevel);
        accumulated += (1.0 - accumulated.a) * voxelSample;
        distance += diameter * 0.5;
    }
    return accumulated.rgb;
}

vec3 computeIndirectLight(vec3 worldPos, vec3 normal) {
    vec3 indirect = vec3(0.0);
    // Normal etrafında 5-6 koni fırlatılır (yarı küre örneklemesi)
    for (int i = 0; i < CONE_COUNT; i++) {
        vec3 coneDir = getConeDirection(normal, i);
        indirect += traceCone(worldPos, coneDir, CONE_APERTURE) * cosineWeight(coneDir, normal);
    }
    return indirect / float(CONE_COUNT);
}
```

Bu sonuç, Faz 2'deki `computeCookTorranceBRDF()` çıktısına ek olarak toplanıyor — yani GI, mevcut PBR pipeline'ını değiştirmiyor, üzerine ekleniyor.

---

## Bölüm B — Post-Processing Pipeline

### B.1 Mimari: Ping-pong framebuffer zinciri

Post-processing efektleri (bloom, tonemap, ambient occlusion, vb.) birbiri ardına uygulanan geçişlerdir. Standart teknik, iki framebuffer arasında "ping-pong" yapmak — her efekt bir önceki efektin çıktısını girdi olarak alır:

```cpp
// Engine/Renderer/PostProcess/PostProcessPipeline.h

class PostProcessPipeline {
public:
    bgfx::TextureHandle process(bgfx::TextureHandle sceneColor) {
        bgfx::TextureHandle current = sceneColor;

        if (ssaoEnabled)   current = ssaoPass.apply(current, gBufferNormals, gBufferDepth);
        if (bloomEnabled)  current = bloomPass.apply(current);
        current = tonemapPass.apply(current); // HDR -> LDR dönüşümü, her zaman son adımlardan biri
        if (fxaaEnabled)   current = fxaaPass.apply(current); // Kenar yumuşatma

        return current;
    }

private:
    SSAOPass ssaoPass;
    BloomPass bloomPass;
    TonemapPass tonemapPass;
    FXAAPass fxaaPass;
    bool ssaoEnabled = true, bloomEnabled = true, fxaaEnabled = true;
};
```

### B.2 Bloom implementasyonu (somut örnek — diğerleri benzer desende)

```cpp
class BloomPass {
public:
    bgfx::TextureHandle apply(bgfx::TextureHandle input) {
        // 1. Parlaklık eşiği üstündeki pikselleri ayıkla
        bgfx::TextureHandle bright = brightnessThresholdShader.render(input, threshold);

        // 2. Bulanıklaştırma zinciri — düşürülmüş çözünürlükte, çok daha ucuz
        bgfx::TextureHandle blurred = bright;
        for (int i = 0; i < downsampleSteps; i++) {
            blurred = downsampleAndBlur(blurred); // Her adımda çözünürlük yarıya iner
        }
        for (int i = 0; i < downsampleSteps; i++) {
            blurred = upsampleAndBlur(blurred); // Geri büyütülür (bu, Unreal'ın kullandığı "dual filtering" tekniği)
        }

        // 3. Orijinal görüntüyle additive blend
        return additiveCombineShader.render(input, blurred, bloomIntensity);
    }

private:
    int downsampleSteps = 5;
    float threshold = 1.0f, bloomIntensity = 0.3f;
};
```

**Neden düşürülmüş çözünürlükte bulanıklaştırma:** Tam çözünürlükte (örn. 1920x1080) bir Gauss blur uygulamak GPU'yu ciddi yorar. Görüntüyü önce küçültüp (downsample) bulanıklaştırıp sonra büyütmek (upsample), gözle neredeyse ayırt edilemez bir kalitede, çok daha ucuza aynı sonucu verir — bu, Unreal ve çoğu modern motorun kullandığı standart optimizasyon.

---

## Bölüm C — LOD Sistemi (Nanite'a İlham, Basitleştirilmiş)

### C.1 Gerçekçi hedef netleştirmesi

Nanite'ın gerçek gücü, mesh'leri **meshlet** denen ~128 üçgenlik kümelere bölüp, her meshlet'i bağımsız olarak, piksel başına yaklaşık bir üçgen düşecek şekilde optimize etmesidir. Bu, GPU-driven bir pipeline (görünürlük kararlarının CPU değil GPU'da compute shader'larla alınması) gerektirir ve Epic Games'in yıllar süren mühendisliğinin ürünüdür.

**Faz 7'de gerçekçi hedef:** Klasik, ayrık LOD seviyeleri (Discrete LOD) — her mesh için 3-4 farklı detay seviyesi önceden üretilir, kameraya olan mesafeye göre CPU tarafında hangisinin kullanılacağı seçilir. Bu, Unreal 4 döneminde (Nanite öncesi) ve hâlâ Unity'de kullanılan standart tekniktir — Nanite kalitesine ulaşmaz ama "ayakları yere basan" bir çözümdür.

### C.2 LOD seviyesi seçimi

```cpp
// Engine/Renderer/LOD/LODSelector.h

struct LODLevel {
    MeshHandle mesh;       // Bu seviyeye ait, önceden basitleştirilmiş (decimated) mesh
    float screenSizeThreshold; // Bu ekran-alanı yüzdesinin altına düşünce bir sonraki LOD'a geç
};

struct LODGroup {
    std::vector<LODLevel> levels; // levels[0] = en detaylı, levels.back() = en basit
};

MeshHandle selectLOD(const LODGroup& group, const RenderProxy& proxy, const Camera& camera) {
    float screenSize = computeScreenSpaceSize(proxy.boundsCenter, proxy.boundsRadius, camera);

    for (auto& level : group.levels) {
        if (screenSize >= level.screenSizeThreshold) return level.mesh;
    }
    return group.levels.back().mesh; // Hiçbiri eşleşmezse en basit seviye
}
```

`computeScreenSpaceSize`, bir objenin ekranda kapladığı alanı (bounding sphere'in projeksiyonu üzerinden) hesaplıyor — mesafe yerine ekran-alanı kullanmak, geniş açılı (FOV) bir kamerada veya çok büyük objelerde daha doğru sonuç veriyor.

### C.3 LOD mesh'lerinin üretimi — hazır kütüphane kullanımı

Mesh basitleştirme (decimation) algoritmasını sıfırdan yazmak, bu projenin "kimliğine" katkı sağlamayan, matematiksel olarak zorlu bir alan — bu yüzden **hazır alınıyor**: **meshoptimizer** (zeux/meshoptimizer, açık kaynak, MIT lisanslı) kütüphanesi, mesh simplification için endüstri standardı bir araç ve zaten birçok üretim motorunda kullanılıyor.

```cpp
// Asset import sırasında (Faz 2'nin Assets/Importers modülüne eklenir)

std::vector<LODLevel> generateLODChain(const MeshData& sourceMesh) {
    std::vector<LODLevel> levels;
    levels.push_back({sourceMesh.handle, 1.0f}); // LOD0 — orijinal, ekranın %100'ünde kullanılır

    float ratios[] = {0.5f, 0.25f, 0.1f}; // Her seviye bir öncekinin üçgen sayısının bu oranı kadar
    float thresholds[] = {0.3f, 0.1f, 0.02f};

    MeshData current = sourceMesh;
    for (int i = 0; i < 3; i++) {
        size_t targetIndexCount = current.indices.size() * ratios[i];
        std::vector<uint32_t> simplified(current.indices.size());

        size_t newCount = meshopt_simplify(
            simplified.data(), current.indices.data(), current.indices.size(),
            current.vertices.data(), current.vertexCount, sizeof(Vertex),
            targetIndexCount, /*targetError=*/0.02f
        );
        simplified.resize(newCount);

        MeshHandle lodHandle = registerMesh(simplified, current.vertices);
        levels.push_back({lodHandle, thresholds[i]});
        current.indices = simplified;
    }
    return levels;
}
```

**Bu adımın önemi:** LOD zinciri, mesh **import edilirken bir kez** üretilir (asset pipeline'ın bir parçası olarak), her runtime'da yeniden hesaplanmaz. Bu, Faz 2'nin Asset sistemine doğal bir genişleme.

---

## Bölüm D — Node-Based Materyal Editörü

### D.1 Neden node-based, neden şimdi mümkün

Faz 4'te Properties panelini reflection üzerinden otomatik ürettiğimiz gibi, node-based materyal editörü de benzer bir "veri odaklı UI" felsefesiyle inşa edilir — ama burada reflection yerine bir **shader graph** veri yapısı kullanılıyor.

```cpp
// Engine/Renderer/Materials/ShaderGraph.h

struct ShaderNode {
    enum class Type { TextureSample, Multiply, Add, Lerp, VertexNormal, OutputAlbedo, /* ... */ };
    Type type;
    std::vector<uint32_t> inputConnections; // Hangi node'lardan veri alıyor
    std::any parameters; // Node'a özel sabit değerler (örn. Multiply'ın çarpanı)
};

struct ShaderGraph {
    std::vector<ShaderNode> nodes;
    uint32_t outputNodeIndex;
};
```

### D.2 Graph'tan gerçek shader koduna derleme

Bu, sistemin en teknik parçası — kullanıcının çizdiği node grafiği, gerçek bir bgfx `.sc` shader dosyasına (GLSL benzeri) dönüştürülmeli:

```cpp
// Engine/Renderer/Materials/ShaderGraphCompiler.h

class ShaderGraphCompiler {
public:
    std::string compileToGLSL(const ShaderGraph& graph) {
        std::string code = "void main() {\n";
        std::unordered_map<uint32_t, std::string> nodeVarNames;

        // Topolojik sıralama — bir node'un girdileri, kendisinden önce hesaplanmalı
        auto sortedNodes = topologicalSort(graph);

        for (uint32_t nodeIdx : sortedNodes) {
            const ShaderNode& node = graph.nodes[nodeIdx];
            std::string varName = "n" + std::to_string(nodeIdx);

            switch (node.type) {
                case ShaderNode::Type::TextureSample:
                    code += "vec4 " + varName + " = texture2D(u_tex" + std::to_string(nodeIdx) + ", v_uv);\n";
                    break;
                case ShaderNode::Type::Multiply: {
                    auto [a, b] = getInputVarNames(node, nodeVarNames);
                    code += "vec4 " + varName + " = " + a + " * " + b + ";\n";
                    break;
                }
                // ... diğer node tipleri için benzer switch dalları
            }
            nodeVarNames[nodeIdx] = varName;
        }

        code += "gl_FragColor = " + nodeVarNames[graph.outputNodeIndex] + ";\n}";
        return code;
    }
};
```

**Kritik pratik not:** Bu derlenmiş GLSL kodu, her frame değil, **kullanıcı graph'ı düzenlediğinde bir kez** derlenip bgfx shader'ına (`.sc` → bgfx'in kendi shaderc aracıyla bytecode'a) çevrilir ve cache'lenir. Runtime'da hiçbir graph yorumlaması olmaz — sadece önceden derlenmiş shader çalışır. Bu, Unreal'ın Material Editor'ünün de kullandığı temel prensiptir.

---

## Bölüm E — Faz 7'nin Diğer Fazlardan Farkı: Sürekli İyileştirme Modeli

Önceki fazların aksine, Faz 7'nin bir "bitiş çizgisi" yok. Bu yüzden kontrol listesi yerine, **aşamalı bir olgunlaşma yol haritası** öneriliyor:

| Seviye | GI | Gölge | LOD | Post-Process |
|---|---|---|---|---|
| **7.1 (MVP)** | SSGI (ekran-uzayı, ucuz) | Tek cascade (Faz 2) | Yok | Bloom + Tonemap |
| **7.2** | VCT eklenir | 2-3 cascade CSM | Discrete LOD (Bölüm C) | + SSAO + FXAA |
| **7.3** | VCT optimizasyonu (sparse voxel) | Contact shadows eklenir | LOD geçişleri yumuşatılır (dithered transition) | + Motion blur, DOF |
| **7.4+** | Yansımalar için SSR eklenir | Temporal gölge filtreleme | — | Temporal Anti-Aliasing (TAA) |

Bu tablo, ekibin (ya da tek geliştiricinin) "yeterince iyi" noktasını kendi zaman/kalite dengesine göre seçebilmesi için tasarlandı — her seviye kendi içinde çalışan, oynanabilir bir motor sunuyor.

---

## Bölüm F — Faz 7.1 (MVP Seviyesi) "Definition of Done" Kontrol Listesi

- [ ] SSGI çalışıyor — kapalı bir odada, doğrudan ışık almayan yüzeylerde görünür dolaylı aydınlatma var
- [ ] Bloom, parlak/emissive yüzeylerde (örn. neon materyal) doğru çalışıyor, aşırı "yıkanma" (over-bloom) olmuyor
- [ ] Tonemap (örn. ACES Filmic) HDR renk aralığını doğru LDR'ye sıkıştırıyor — çok parlak sahnelerde detay kaybı minimum
- [ ] Discrete LOD sistemi çalışıyor — kameradan uzaklaştıkça mesh'in daha basit versiyonu render ediliyor, geçişler göz yorucu "pop" etkisi yaratmıyor (mümkünse dithered transition ile yumuşatılmış)
- [ ] meshoptimizer entegrasyonu asset import pipeline'ına bağlanmış, her mesh import edilirken otomatik LOD zinciri üretiliyor
- [ ] Node-based materyal editörü temel node'larla (Texture Sample, Multiply, Add, Lerp) çalışıyor ve graph değişince shader otomatik yeniden derleniyor
- [ ] Performans testi: VCT/SSGI açıkken orta seviye bir GPU'da (örn. GTX 1660 sınıfı) 1080p'de kabul edilebilir FPS (>60) korunuyor

---

## Sonraki Adım Önerisi

Faz 7'nin MVP seviyesi tamamlandığında motor artık görsel olarak "Unreal'a yaklaşıyoruz" diyebileceğimiz bir noktaya geliyor. Üç yön öneriyorum:

1. **Faz 8 — C# desteği:** Grafik kalitesi belli bir olgunluğa ulaştığına göre, deneyimli geliştiricileri projeye dahil etmek için C# binding'inin (Faz 1.5'teki reflection sistemi üzerinden, Luau binding'ine paralel bir mantıkla) kurulması.
2. **Karakter kontrolcüsü:** Hâlâ bekleyen bir konu — artık görsel kalite de yerine oturduğuna göre oynanabilirlik tarafına dönmek mantıklı olabilir.
3. **Faz 3'teki script timeout konusu:** Her fazda "öncelik arttı" notunu düşüyoruz — artık gerçekten ele alınması gereken bir teknik borç haline geldi.

Hangisiyle devam edelim?
