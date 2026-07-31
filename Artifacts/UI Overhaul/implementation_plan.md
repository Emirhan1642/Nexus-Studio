# Nexus Studio — UI Overhaul: Final Plan (v4)

> 7 mimari sorun düzeltildi. Bu plan artık çelişki içermiyor ve uygulamaya hazır.

---

## Düzeltme Özeti

| # | Sorun | Karar |
|---|---|---|
| 1 | imgui shader derleme belirsizliği | **Precompiled `.bin` → C++ header olarak embed** — shaderc runtime'da gerekmez |
| 2 | DockSpace vs EditorLayout çakışması | **DockSpace tek otorite** — DockBuilder ile ilk layout, sonra ImGui yönetir |
| 3 | DockSpace TopBar içinde | **DockSpace ImGuiLayer::beginFrame() içinde** — TopBar sadece menü çizer |
| 4 | `bgfx::createTexture2D` allocator sorunu | **`bgfx::copy()` kullan** — allocator gereksiz hale gelir |
| 5 | `bgfx::copy()` her frame heap allocation | **Transient buffer API** — per-frame pool'dan sıfır malloc |
| 6 | `imgui.ini` tek kayıt, preset desteği yok | **Named preset sistemi** — `layouts/` klasörü + `ImGui::LoadIniSettingsFromDisk` |
| 7 | `m_firstRun` bayrağı unreliable | **`!fs::exists(io.IniFilename)` kontrolü** — ini dosyası yoksa default layout kur |

---

## Mevcut Durumu Anlamak (CMake Analizi)

`Editor/CMakeLists.txt` şu an şunları link eder:
- `example-common` → bgfx'in bundled imgui wrapper'ı (`imguiCreate`)
- `bgfx/3rdparty/dear-imgui` → bgfx'in gömülü (docking-less) ImGui fork'u

Geçiş sonrasında:
- `example-common` bağlantısı **kesilir** (yalnızca render helper'lar için gerekirse bx/bimg kalır)
- Dear ImGui **Docking branch** ayrı bir FetchContent olarak gelir
- `imgui_impl_glfw` Dear ImGui'nin kendi backend'i olarak gelir
- Custom `imgui_impl_bgfx` bizim yazdığımız dosya olur

---

## Adım 0 — bgfx Backend Geçişi

### 0-A: ThirdParty/CMakeLists.txt — Dear ImGui Docking Branch Ekle

```cmake
# ThirdParty/CMakeLists.txt'e eklenir
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking  # ★ docking branch
)
FetchContent_MakeAvailable(imgui)

add_library(dear_imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    # imgui_impl_bgfx.cpp bizim dosyamız, Editor'da derlenir
)
target_include_directories(dear_imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(dear_imgui PUBLIC glfw)
```

### 0-B: imgui Shader Stratejisi — Precompile + Embed

> [!IMPORTANT]
> bgfx shader'ları platform başına farklı binary üretir (DX12=DXBC, Vulkan=SPIR-V).
> `shaderc`'yi CMake'e eklemek mümkün ama geliştirme döneminde gereksiz karmaşıklık.
>
> **Seçilen yaklaşım:** imgui vertex/fragment shader'larını **bir kez elle precompile** edip
> `Assets/Shaders/compiled/` altına `.bin` olarak koy, repoya commit et.
> Sonra bu dosyaları `xxd -i` veya basit bir Python scriptiyle C++ header'ına embed et.
> Böylece runtime'da dosya sistemi erişimine gerek kalmaz, shader her zaman binary içinde.

**Shader kaynakları** (bgfx repo'sundan alınır, değiştirilmez):
- `bgfx/examples/common/imgui/vs_ocornut_imgui.sc`
- `bgfx/examples/common/imgui/fs_ocornut_imgui.sc`

**Derleme komutu** (bir kez, geliştirici tarafından çalıştırılır):
```powershell
# DX12 için (Windows'ta geliştirme):
shaderc -f vs_ocornut_imgui.sc -o vs_imgui_dx12.bin --type v --platform windows -p s_5_0
shaderc -f fs_ocornut_imgui.sc -o fs_imgui_dx12.bin --type f --platform windows -p s_5_0
```

**Embed scripti** (`Tools/embed_shader.py`):
```python
# Assets/Shaders/compiled/imgui_shaders.h üretir
# Binary array olarak C++ header'ına gömer
with open("vs_imgui_dx12.bin","rb") as f:
    data = f.read()
print("static const uint8_t vs_imgui[] = {" + ",".join(str(b) for b in data) + "};")
```

Üretilen `imgui_shaders.h` repoya commit edilir. `imgui_impl_bgfx.cpp` bu header'ı include eder:
```cpp
#include "imgui_shaders.h"
// ...
const bgfx::Memory* vsMem = bgfx::makeRef(vs_imgui, sizeof(vs_imgui));
bgfx::ShaderHandle vs = bgfx::createShader(vsMem);
```

**Sonuç:** CMake'e `add_custom_command` gerekmez. Shader güncellenmesi gerekirse script tekrar çalıştırılıp header commit edilir.

### 0-C: `imgui_impl_bgfx.h` + `imgui_impl_bgfx.cpp`

**API:**
```cpp
bool ImGui_ImplBgfx_Init(uint8_t viewId);
void ImGui_ImplBgfx_Shutdown();
void ImGui_ImplBgfx_NewFrame();
void ImGui_ImplBgfx_RenderDrawData(ImDrawData* drawData);
```

**Vertex layout:**
```cpp
bgfx::VertexLayout layout;
layout.begin()
    .add(bgfx::Attrib::Position,  2, bgfx::AttribType::Float)
    .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
    .add(bgfx::Attrib::Color0,    4, bgfx::AttribType::Uint8, true)
    .end();
```

**Font texture yükleme — allocator sorununu çözen yaklaşım:**
```cpp
// ★ bgfx::copy() kullanılır — hiçbir allocator gerekmez
unsigned char* pixels;
int width, height;
ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

const bgfx::Memory* mem = bgfx::copy(pixels, width * height * 4);
// bgfx::copy() kendi içinde bellek ayırır ve ownership'i alır
// bx::AllocatorI* parametresi gerekmez

m_fontTexture = bgfx::createTexture2D(
    (uint16_t)width, (uint16_t)height,
    false, 1,
    bgfx::TextureFormat::RGBA8,
    0,
    mem  // ★ allocator parametresi yok, memory ref geçiliyor
);
```

**RenderDrawData — Transient Buffer API:**

> [!IMPORTANT]
> `bgfx::copy()` her frame heap'ten bellek ayırır. `allocTransientVertexBuffer` /
> `allocTransientIndexBuffer` ise bgfx'in **per-frame transient pool**'undan alır —
> frame sonunda otomatik serbest kalır, malloc/free sıfır.

```cpp
for (int n = 0; n < drawData->CmdListsCount; n++) {
    const ImDrawList* cmdList = drawData->CmdLists[n];
    uint32_t numVtx = (uint32_t)cmdList->VtxBuffer.Size;
    uint32_t numIdx = (uint32_t)cmdList->IdxBuffer.Size;

    // ★ Transient buffer — frame sonunda otomatik serbest, malloc yok
    bgfx::TransientVertexBuffer tvb;
    bgfx::TransientIndexBuffer  tib;

    if (!bgfx::allocTransientBuffers(&tvb, m_layout, numVtx, &tib, numIdx))
        break;  // Transient havuz doluysa o frame'i atla

    // CPU tarafında doğrudan yaz — copy yok
    std::memcpy(tvb.data, cmdList->VtxBuffer.Data, numVtx * sizeof(ImDrawVert));
    std::memcpy(tib.data, cmdList->IdxBuffer.Data, numIdx * sizeof(ImDrawIdx));

    uint32_t idxOffset = 0;
    for (const ImDrawCmd& cmd : cmdList->CmdBuffer) {
        if (cmd.UserCallback) { cmd.UserCallback(cmdList, &cmd); idxOffset += cmd.ElemCount; continue; }
        bgfx::setScissor((uint16_t)cmd.ClipRect.x, (uint16_t)cmd.ClipRect.y,
                         (uint16_t)(cmd.ClipRect.z - cmd.ClipRect.x),
                         (uint16_t)(cmd.ClipRect.w - cmd.ClipRect.y));
        bgfx::setTexture(0, m_uniformTexture,
                         (bgfx::TextureHandle){(uint16_t)(uintptr_t)cmd.TextureId});
        bgfx::setVertexBuffer(0, &tvb, cmd.VtxOffset, numVtx);  // transient
        bgfx::setIndexBuffer(&tib, idxOffset, cmd.ElemCount);    // transient
        bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA);
        bgfx::submit(m_viewId, m_program);
        idxOffset += cmd.ElemCount;
    }
}
// ★ tvb/tib belleklerini serbest bırakmaya gerek yok — bgfx frame sonunda halleder
```

**Not:** `bgfx::allocTransientBuffers` (tekil çağrı, hem vertex hem index) atomik olarak
ayırır — biri başarısız olursa ikisi de başarısız, yarım state olmaz.

### 0-D: `ImGuiLayer.cpp` — Tamamen Yeniden Yaz

```cpp
void ImGuiLayer::init(GLFWwindow* window) {
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // ★

    // Font yükleme (Inter veya fallback)
    const char* fontPath = "Assets/Fonts/Inter-Regular.ttf";
    if (std::filesystem::exists(fontPath))
        io.Fonts->AddFontFromFileTTF(fontPath, 13.0f);
    // Yoksa ImGui'nin varsayılan Proggy Clean kullanılır

    ImGui_ImplGlfw_InitForOther(window, true);  // renderer-agnostic
    ImGui_ImplBgfx_Init(VIEW_ID_IMGUI);          // bgfx view 255
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplBgfx_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // ★ DockSpace BURADA açılır — TopBar veya başka bir panel değil
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##DockSpaceHost", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove     |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar);  // ★ MenuBar flag — TopBar bu alana çizilir
    ImGui::PopStyleVar(3);

    m_dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(m_dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);

    // İlk kez çalışıyorsa default layout kur (DockBuilder API)
    if (m_firstRun) {
        buildDefaultLayout(m_dockspaceId, vp->WorkSize);
        m_firstRun = false;
    }
    // DockSpaceHost penceresi endFrame()'de kapatılır
}

void ImGuiLayer::endFrame() {
    ImGui::End();  // DockSpaceHost'u kapat
    ImGui::Render();
    ImGui_ImplBgfx_RenderDrawData(ImGui::GetDrawData());
}
```

---

## Adım 1 — Layout Mimarisi: DockBuilder ile Default Layout

> [!IMPORTANT]
> **Bu planın en kritik mimarı kararı:**
> DockSpace tek layout otoritesidir. `EditorLayout::computeRects()` yoktur.
> Her panel `ImGui::Begin("Explorer")` yazar, DockSpace onun nereye gittiğini yönetir.
> `EditorLayout` yalnızca **görünürlük (show/hide)** ve **ilk run default boyutları** tutar.

### DockBuilder ile Default Layout — Güvenli Guard

> [!IMPORTANT]
> `m_firstRun` bayrağı unreliable: kullanıcı layoutu sıfırlayıp editörü kapatırsa
> bir sonraki açılışta tekrar default'a dönülür. **Doğru guard: `imgui.ini`'nin
> disk'te var olup olmadığını kontrol et.** Dosya varsa ImGui zaten yükler, biz karışmayız.

`ImGuiLayer::beginFrame()` içindeki DockSpace sonrasında:
```cpp
// ★ imgui.ini yoksa (ilk çalıştırma) default layout kur
// ini varsa ImGui kendi yükler — biz hiç dokunmayız
ImGuiIO& io = ImGui::GetIO();
if (io.IniFilename && !std::filesystem::exists(io.IniFilename)) {
    buildDefaultLayout(m_dockspaceId, vp->WorkSize);
}
// m_firstRun kullanılmaz
```

`ImGuiLayer::buildDefaultLayout()`:
```cpp
void ImGuiLayer::buildDefaultLayout(ImGuiID dockspaceId, ImVec2 size) {
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, size);

    ImGuiID main      = dockspaceId;
    ImGuiID left      = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Left,  0.037f, nullptr, &main);
    ImGuiID right     = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Right, 0.27f,  nullptr, &main);
    ImGuiID ai        = ImGui::DockBuilderSplitNode(right, ImGuiDir_Right, 0.45f,  nullptr, &right);
    ImGuiID bottom    = ImGui::DockBuilderSplitNode(main,  ImGuiDir_Down,  0.28f,  nullptr, &main);
    ImGuiID centerTop = main;
    ImGuiID centerMid = ImGui::DockBuilderSplitNode(centerTop, ImGuiDir_Down, 0.47f, nullptr, &centerTop);
    ImGuiID explorer  = right;
    ImGuiID properties = ImGui::DockBuilderSplitNode(explorer, ImGuiDir_Down, 0.62f, nullptr, &explorer);

    ImGui::DockBuilderDockWindow("##LeftToolbar",   left);
    ImGui::DockBuilderDockWindow("Viewport",        centerTop);
    ImGui::DockBuilderDockWindow("Material Editor", centerMid);
    ImGui::DockBuilderDockWindow("Asset Browser",   bottom);
    ImGui::DockBuilderDockWindow("Explorer",        explorer);
    ImGui::DockBuilderDockWindow("Properties",      properties);
    ImGui::DockBuilderDockWindow("AI Copilot",      ai);

    ImGui::DockBuilderFinish(dockspaceId);
}
```

Bu layout kullanıcı tarafından sürüklenip değiştirilebilir. ImGui bunu `imgui.ini` dosyasına otomatik kaydeder.

### EditorLayout — Named Preset Sistemi

> [!NOTE]
> `imgui.ini` aktif layout'u tutar. İleride "Game Dev Layout" / "Artist Layout" gibi
> preset'ler eklemek gerektiğinde `ImGui::SaveIniSettingsToDisk` /
> `ImGui::LoadIniSettingsFromDisk` API'si üzerinden `layouts/` klasörü kullanılır.
> Bu adım şu an implemente edilmez, sadece mimari olarak hazır bırakılır.

```cpp
class EditorLayout {
public:
    static EditorLayout& instance();

    // Sadece görünürlük — paneller bu flag'e göre Begin()/End() yapar
    bool showLeftToolbar    = true;
    bool showMaterialEditor = true;
    bool showAssetBrowser   = true;
    bool showAICopilot      = true;
    // Explorer ve Properties her zaman görünür

    // ★ Preset API (ileride kullanılacak, şimdi sadece tanımlanır)
    void savePreset(const char* name) {
        // layouts/<name>.ini'ye ImGui ini içeriğini yazar
        std::string path = std::string("layouts/") + name + ".ini";
        ImGui::SaveIniSettingsToDisk(path.c_str());
    }
    void loadPreset(const char* name) {
        // layouts/<name>.ini'yi yükler, imgui.ini'yi günceller
        std::string path = std::string("layouts/") + name + ".ini";
        if (std::filesystem::exists(path))
            ImGui::LoadIniSettingsFromDisk(path.c_str());
    }
    // Hazır preset'ler (ileride TopBar View menüsüne bağlanır):
    // "Default", "GameDev", "Artist", "Minimal"
};
```

---

## Adım 2 — NexusTheme

#### [NEW] `Editor/UI/NexusTheme.h` + `NexusTheme.cpp`

`apply()` fonksiyonu `ImGuiLayer::init()`'in sonunda **bir kez** çağrılır:
```cpp
// Temel renkler (HTML palette'ten)
ImGui::StyleColorsDark();  // Sıfırlama
ImGuiStyle& s = ImGui::GetStyle();
s.Colors[ImGuiCol_WindowBg]     = HexColor(0x0e0e0e);
s.Colors[ImGuiCol_ChildBg]      = HexColor(0x050505);
s.Colors[ImGuiCol_PopupBg]      = HexColor(0x0e0e0e);
s.Colors[ImGuiCol_Border]       = HexColor(0x242424);
s.Colors[ImGuiCol_FrameBg]      = HexColor(0x171717);
s.Colors[ImGuiCol_TitleBg]      = HexColor(0x0e0e0e);
s.Colors[ImGuiCol_TitleBgActive]= HexColor(0x0e0e0e);
s.Colors[ImGuiCol_MenuBarBg]    = HexColor(0x0e0e0e);
s.Colors[ImGuiCol_Header]       = ImVec4(0, 0.82f, 1, 0.15f);   // accent/15
s.Colors[ImGuiCol_HeaderHovered]= ImVec4(0, 0.82f, 1, 0.25f);
s.Colors[ImGuiCol_Tab]          = HexColor(0x0e0e0e);
s.Colors[ImGuiCol_TabActive]    = HexColor(0x050505);
s.Colors[ImGuiCol_TabHovered]   = HexColor(0x171717);
s.Colors[ImGuiCol_Button]       = HexColor(0x171717);
s.Colors[ImGuiCol_ButtonHovered]= HexColor(0x242424);
s.Colors[ImGuiCol_CheckMark]    = HexColor(0x00d2ff);
s.Colors[ImGuiCol_SliderGrab]   = HexColor(0x00d2ff);
s.Colors[ImGuiCol_DockingPreview] = ImVec4(0, 0.82f, 1, 0.3f);

s.WindowRounding   = 0.0f;
s.FrameRounding    = 3.0f;
s.GrabRounding     = 3.0f;
s.TabRounding      = 0.0f;
s.WindowBorderSize = 1.0f;
s.FrameBorderSize  = 0.0f;
s.IndentSpacing    = 12.0f;
s.ItemSpacing      = ImVec2(4, 3);
s.FramePadding     = ImVec2(6, 3);
```

---

## Adım 3 — TopBar

#### [NEW] `Editor/UI/TopBar.h` + `TopBar.cpp`

TopBar, `ImGuiLayer::beginFrame()`'deki `ImGui::Begin("##DockSpaceHost", ..., MenuBar)` penceresinin **menu bar alanına** çizer. Kendi DockSpace sorumluluğu yoktur.

```cpp
void TopBar::draw(bool isSimulating, bool& toggleSim) {
    // DockSpaceHost penceresi zaten açık (ImGuiLayer'da açıldı)
    if (!ImGui::BeginMenuBar()) return;

    // Logo
    ImGui::Image(IconRegistry::instance().get("logo_nexus"), ImVec2(18, 18));
    ImGui::SameLine();
    ImGui::TextColored(accent, "NEXUS");

    ImGui::Separator();

    // Menüler
    if (ImGui::BeginMenu("File"))   { /* ... */ ImGui::EndMenu(); }
    if (ImGui::BeginMenu("Edit"))   { /* ... */ ImGui::EndMenu(); }
    if (ImGui::BeginMenu("View"))   { /* ... */ ImGui::EndMenu(); }
    // Scene, Object, Material, Physics, Plugins...

    // Sağa hizala — Play kontrolleri
    float rightWidth = 220.0f;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - rightWidth);

    // Build target dropdown
    ImGui::PushStyleColor(ImGuiCol_Button, panelColor);
    ImGui::Button("PC / DX12  v", ImVec2(90, 0));
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Play / Stop butonu — glow efekti DrawList ile
    ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,1));
    if (ImGui::Button(isSimulating ? "  Stop" : "  Play", ImVec2(70, 0)))
        toggleSim = true;
    // DrawList ile glow: AddRectFilledMultiColor veya AddShadowRect
    ImGui::PopStyleColor(2);

    ImGui::EndMenuBar();
}
```

---

## Adım 4 — LeftToolbar

#### [NEW] `Editor/UI/LeftToolbar.h` + `LeftToolbar.cpp`

`ImGui::Begin("##LeftToolbar")` ile çizilir — bu pencere DockBuilder'da `left` düğümüne sabitlenmiş.  
`ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNav`

Buton genişliği 28px, yükseklik 28px. İkonlar `IconRegistry::get(name)` ile.

Aktif araç vurgusu:
```cpp
// DrawList ile accent glow simülasyonu
if (currentTool == Tool::Move) {
    ImVec2 p = ImGui::GetItemRectMin();
    ImVec2 q = ImGui::GetItemRectMax();
    drawList->AddRectFilled(p, q, IM_COL32(0, 210, 255, 38), 3.0f);   // accent/15
    drawList->AddRect(p, q, IM_COL32(0, 210, 255, 128), 3.0f, 0, 1.0f); // accent/50 border
}
```

---

## Adım 5 — Viewport Panel (Update)

`ImGui::Begin("Viewport")` — DockBuilder'da `centerTop` düğümüne sabitlenmiş.

Eklenenler (render boyutu değişmez — sadece DrawList üzerine overlay):

1. **Viewport Toolbar** (DrawList, h=26px):
   - Sol: "Perspective ▾" | "Lit (PBR) ▾" | World/Local toggle
   - Sağ: kamera hızı | wireframe ikonu | collision ikonu | FPS

2. **Orientation Cube** (top-right, 40×40px, DrawList):
   ```cpp
   ImVec2 cubePos = ImVec2(panelMax.x - 50, panelMin.y + 10);
   drawList->AddRectFilled(cubePos, ImVec2(cubePos.x+40, cubePos.y+40),
                            IM_COL32(14, 14, 14, 220), 4.0f);
   // TOP / FRONT / RIGHT metinleri AddText ile
   // InvisibleButton ile click detection → kamera snap
   ```

3. **Status bar** (DrawList, h=20px, alt): seçili nesne + frame time.

---

## Adım 6 — Explorer Panel (Update)

`ImGui::Begin("Explorer")` — DockBuilder'da `explorer` düğümüne.

Seçim vurgusu (DrawList ile):
```cpp
if (isSelected) {
    ImVec2 rowMin = ImGui::GetItemRectMin();
    ImVec2 rowMax = ImGui::GetItemRectMax();
    // Arka plan dolgusu
    drawList->AddRectFilled(rowMin, rowMax, IM_COL32(0, 210, 255, 38));
    // Sol border çizgisi
    drawList->AddLine(rowMin, ImVec2(rowMin.x, rowMax.y),
                      IM_COL32(0, 210, 255, 255), 2.0f);
}
```

Tip renk çubuğu (her node için):
```cpp
ImVec2 barMin = ImGui::GetItemRectMin();
drawList->AddRectFilled(barMin, ImVec2(barMin.x + 3, barMin.y + 16),
                         typeColor, 1.0f);  // 3px renkli bar
```

---

## Adım 7 — Properties Panel (Update)

`ImGui::Begin("Properties")` — DockBuilder'da `properties` düğümüne.

Toggle pill (DrawList):
```cpp
void DrawTogglePill(bool value) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 size(24, 14);
    ImU32 bg = value ? IM_COL32(34, 197, 94, 255) : IM_COL32(36, 36, 36, 255);
    drawList->AddRectFilled(p, ImVec2(p.x+size.x, p.y+size.y), bg, size.y/2);
    float cx = value ? p.x + size.x - 8 : p.x + 4;
    drawList->AddCircleFilled(ImVec2(cx, p.y + size.y/2), 5.0f, IM_COL32(255,255,255,255));
    // InvisibleButton ile click detection
}
```

Physics bölümü için turuncu gradient arka plan:
```cpp
if (ImGui::CollapsingHeader("Physics • Rigidbody")) {
    ImVec2 sectionMin = ImGui::GetCursorScreenPos();
    // ... içerik çizilir ...
    ImVec2 sectionMax = ImGui::GetCursorScreenPos();
    // Gradient overlay (AddRectFilledMultiColor)
    drawList->AddRectFilledMultiColor(
        sectionMin, ImVec2(sectionMax.x, sectionMax.y),
        IM_COL32(245, 158, 11, 38), IM_COL32(245, 158, 11, 0),  // turuncu -> şeffaf
        IM_COL32(245, 158, 11, 0),  IM_COL32(245, 158, 11, 38));
}
```

---

## Adım 8 — Asset Browser (Update)

`ImGui::Begin("Asset Browser")` — DockBuilder'da `bottom` düğümüne.

Mevcut kod korunur, şunlar eklenir:
1. Üst kısma `ImGui::TabBar("BottomTabs")`: Asset Manager | Output/Console | Live Profiler.
2. Sol kısma `ImGui::BeginChild("FolderTree", ImVec2(192, 0))` — klasör ağacı.
3. Asset card'lara DrawList ile rounded rect (4px radius).

---

## Adım 9 — AI Copilot Panel (UI Kabuğu)

`ImGui::Begin("AI Copilot")` — DockBuilder'da `ai` düğümüne.

Mesaj gönderme işlevi yok. Statik örnek mesajlar + input bar (placeholder + devre dışı Send butonu).

---

## Adım 10 — IconRegistry

```cpp
// bgfx::copy() — allocator gereksiz
void IconRegistry::load(const char* name, const char* pngPath) {
    int w, h, ch;
    unsigned char* pixels = stbi_load(pngPath, &w, &h, &ch, 4);
    if (!pixels) return;
    
    const bgfx::Memory* mem = bgfx::copy(pixels, w * h * 4);
    stbi_image_free(pixels);  // bgfx::copy kopyayı aldı, orijinal serbest bırakılır
    
    m_icons[name] = bgfx::createTexture2D(
        (uint16_t)w, (uint16_t)h, false, 1,
        bgfx::TextureFormat::RGBA8, 0, mem
    );
}

ImTextureID IconRegistry::get(const char* name) const {
    auto it = m_icons.find(name);
    if (it == m_icons.end()) return (ImTextureID)(uintptr_t)0;
    return (ImTextureID)(uintptr_t)it->second.idx;
}
```

---

## Adım 11 — Editor/CMakeLists.txt Güncelleme

```cmake
set(EDITOR_SOURCES
    Main.cpp
    UI/Backend/imgui_impl_bgfx.cpp
    UI/ImGuiLayer.cpp
    UI/NexusTheme.cpp
    UI/IconRegistry.cpp
    UI/TopBar.cpp
    UI/LeftToolbar.cpp
    UI/ViewportPanel.cpp
    UI/ExplorerPanel.cpp
    UI/PropertiesPanel.cpp
    UI/AssetBrowserPanel.cpp
    UI/MaterialEditorPanel.cpp
    UI/AICopilotPanel.cpp
    Undo/UndoStack.cpp
)

target_include_directories(NexusStudioEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_BINARY_DIR}/_deps/imgui-src         # ★ Yeni Dear ImGui (docking)
    ${CMAKE_BINARY_DIR}/_deps/imgui-src/backends
    ${CMAKE_BINARY_DIR}/_deps/imnodes-src
    # example-common artık gerekmiyor
)

target_link_libraries(NexusStudioEditor PRIVATE
    EngineCore EnginePhysics Jolt EngineRenderer
    EngineScripting EngineAssets EngineNetworking EngineAnimation
    dear_imgui   # ★ Yeni target (ThirdParty'de tanımlı)
    glfw bgfx bimg bx
    # example-common ÇIKARILDI
)
```

---

## Adım 12 — Main.cpp Güncelleme

```cpp
// Yeni nesneler:
TopBar        topBar;
LeftToolbar   leftToolbar;
AICopilotPanel aiCopilot;

// init sonrası:
IconRegistry::instance().loadAll("Assets/Icons");
// NexusTheme::apply() zaten ImGuiLayer::init() içinde çağrıldı

// Ana döngüde:
ImGuiLayer::instance().beginFrame();   // ★ DockSpace burada açılır
  topBar.draw(isSimulating, toggleSim); // MenuBar'a çizer, DockSpace'den habersiz
  leftToolbar.draw();                  // Begin("##LeftToolbar")
  viewport.draw(camera);               // Begin("Viewport")
  explorer.draw();                     // Begin("Explorer")
  properties.draw();                   // Begin("Properties")
  materialEditor.draw(&showMat);       // Begin("Material Editor")
  AssetBrowserPanel::instance().draw();// Begin("Asset Browser")
  if (EditorLayout::instance().showAICopilot)
      aiCopilot.draw();                // Begin("AI Copilot")
ImGuiLayer::instance().endFrame();     // ★ DockSpaceHost kapatılır + render
```

---

## Bağımlılık Sırası

```
[ThirdParty] dear_imgui (docking branch) ekle
     │
     ▼
[Adım 0] imgui_impl_bgfx.cpp + imgui_shaders.h (precompiled)
     │
     ├──> [Adım 1] ImGuiLayer yeniden yaz (DockSpace + DockBuilder)
     │         │
     │         ▼
     │    [Adım 2] NexusTheme (init'te apply)
     │
     ├──> [Adım 10] IconRegistry (bgfx::copy)
     │
     └──> [Adım 3-9] Tüm paneller (sıra bağımsız, paralel çalışılabilir)
               │
               ▼
          [Adım 11] CMakeLists.txt
               │
               ▼
          [Adım 12] Main.cpp
```

---

## Doğrulama Kontrol Listesi

### Adım 0 sonrası (zemin):
- [ ] `cmake --build` sıfır hata
- [ ] `DockingEnable` flag'i `io.ConfigFlags`'te set — ImGui About penceresinde görünür
- [ ] `DockSpaceOverViewport` çalışıyor, pencereler sürüklenebiliyor
- [ ] Font: Inter yüklüyse 13px Inter, yoksa Proggy Clean — iki durum da derleniyor

### Adım 1 sonrası (layout):
- [ ] Default layout ilk çalıştırmada doğru kuruldu
- [ ] Explorer/Properties splitter sürüklenebiliyor (ImGui docking splitter)
- [ ] `imgui.ini` oluştu, editör kapatılıp açılınca layout korunuyor

### Tam entegrasyon:
- [ ] HTML tasarımıyla yan yana karşılaştırma: renk, font, spacing eşleşiyor
- [ ] Orientation Cube tıklanabilir, kamera snap çalışıyor
- [ ] Toggle pill'ler görsel olarak doğru (yeşil = on, gri = off)
- [ ] AI Copilot panel açılıp kapanabiliyor (`showAICopilot` toggle)
