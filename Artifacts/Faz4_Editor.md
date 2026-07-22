# Faz 4 — Teknik Derinlemesine İnceleme
## Editör: ImGui, Viewport, Explorer, Properties ve Undo/Redo

Bu doküman, Faz 1 (Reflection/DataModel), Faz 2 (Render) ve Faz 3'te (Scripting) kurulan sistemlerin üzerine, gerçek anlamda "Roblox Studio hissi" veren editör arayüzünün nasıl inşa edileceğini inceler. Faz 4 tamamlandığında proje MVP noktasına ulaşmış olacak.

---

## Bölüm A — ImGui Entegrasyonunun Temel Prensibi

### A.1 ImGui neden "immediate mode"?

Unreal'ın Slate'i veya Unity'nin UIElements'i **retained mode** çalışır — yani bir buton bir kez oluşturulur, bellekte bir nesne olarak yaşar, her karede tekrar "inşa edilmez". ImGui ise **immediate mode**: her karede tüm arayüz kodu baştan çalışır, hiçbir UI nesnesi bellekte kalıcı olarak saklanmaz.

```cpp
// Bir retained-mode sistemde (kavramsal):
Button* btn = new Button("Save");       // Bir kere oluşturulur
btn->onClick = []() { save(); };        // Callback bağlanır
// ... UI nesnesi bellekte kalıcı yaşar, state'i kendi tutar

// ImGui'de (immediate mode):
if (ImGui::Button("Save")) {            // HER KAREDE bu satır çalışır
    save();                              // true dönerse tıklanmış demektir
}
// Hiçbir "Button nesnesi" bellekte kalmıyor
```

**Bizim için pratik anlamı:** Properties panelinin kodu, "şu anda seçili olan objenin reflection verisini oku, her property için bir input alanı çiz" mantığıyla her karede yeniden çalışır. Yeni bir sınıf (`SpotLight` gibi) eklendiğinde Properties panel kodunda **hiçbir değişiklik gerekmez** — çünkü zaten her karede reflection'dan taze veri okuyor.

### A.2 ImGui'nin bgfx ile entegrasyonu

```cpp
// Editor/UI/ImGuiIntegration.cpp

void ImGuiLayer::initialize(GLFWwindow* window) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOther(window, true);

    // bgfx'e özel bir ImGui backend gerekiyor (resmi bgfx repo'sunda örnek mevcut,
    // imgui/imgui_impl_bgfx.cpp benzeri bir dosya bu projeye özel yazılacak)
    ImGuiBgfx_Init(View_ImGui); // Ayrı bir bgfx view id kullanır (Faz 2'deki RenderView enum'una eklenir)
}

void ImGuiLayer::beginFrame() {
    ImGuiBgfx_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::DockSpaceOverViewport(); // ★ Roblox Studio tarzı sürüklenebilir panel yerleşimi
}

void ImGuiLayer::endFrame() {
    ImGui::Render();
    ImGuiBgfx_RenderDrawData(ImGui::GetDrawData());
}
```

`DockSpaceOverViewport()` kritik — bu, panellerin (Explorer, Properties, Viewport) kullanıcı tarafından sürüklenip yeniden düzenlenebilmesini, Roblox Studio/Unreal/Unity'deki gibi doklanabilir bir arayüz deneyimini tek satırla sağlıyor.

---

## Bölüm B — Viewport: 3D Sahnenin Editör İçinde Görünmesi

### B.1 Temel teknik: Render-to-Texture

Viewport paneli aslında normal bir ImGui penceresi, içine bir **texture** çiziliyor. Bu texture, Faz 2'deki Renderer'ın **ekrana değil, bir framebuffer'a** render etmesiyle elde ediliyor:

```cpp
// Editor/UI/Viewport.h

class ViewportPanel {
public:
    void resize(uint16_t width, uint16_t height) {
        if (width == currentWidth && height == currentHeight) return;

        if (bgfx::isValid(colorTexture)) bgfx::destroy(colorTexture);
        if (bgfx::isValid(frameBuffer)) bgfx::destroy(frameBuffer);

        colorTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_RT);
        bgfx::TextureHandle depthTexture = bgfx::createTexture2D(width, height, false, 1,
            bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);

        bgfx::TextureHandle attachments[] = { colorTexture, depthTexture };
        frameBuffer = bgfx::createFrameBuffer(2, attachments, true);

        currentWidth = width;
        currentHeight = height;
    }

    void draw() {
        ImGui::Begin("Viewport");

        ImVec2 availSize = ImGui::GetContentRegionAvail();
        resize((uint16_t)availSize.x, (uint16_t)availSize.y); // Panel boyutu değişince texture'ı yeniden oluştur

        // Bu karede Renderer'a "ekrana değil, bu framebuffer'a çiz" diyoruz
        Renderer::instance().renderFrame(editorCamera, RenderScene::instance(), frameBuffer);

        // Az önce render edilen texture'ı ImGui penceresi içine bir resim gibi çiziyoruz
        ImGui::Image((ImTextureID)(uintptr_t)colorTexture.idx, availSize);

        handleGizmoInput(); // ★ Bölüm D
        handleCameraInput(); // WASD + sağ tık sürükle ile kamera hareketi

        ImGui::End();
    }

private:
    bgfx::TextureHandle colorTexture = BGFX_INVALID_HANDLE;
    bgfx::FrameBuffer frameBuffer = BGFX_INVALID_HANDLE;
    uint16_t currentWidth = 0, currentHeight = 0;
    Camera editorCamera;
};
```

**Bu tasarımın önemi:** Faz 2'de Renderer'ı tasarlarken bilinçli olarak "hedef framebuffer" parametrik bırakılmıştı (doğrudan ekrana yazmak yerine). Bu sayede aynı Renderer kodu hem editör viewport'una hem (Runtime/Player'da) doğrudan ekrana render edebiliyor — kod tekrarı yok.

---

## Bölüm C — Explorer ve Properties Panelleri

### C.1 Explorer — DataModel ağacının görsel karşılığı

```cpp
// Editor/UI/ExplorerPanel.h

class ExplorerPanel {
public:
    void draw() {
        ImGui::Begin("Explorer");
        drawInstanceNode(DataModel::instance().shared_from_this());
        ImGui::End();
    }

private:
    void drawInstanceNode(const std::shared_ptr<Instance>& inst) {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (inst == SelectionManager::instance().getSelected())
            flags |= ImGuiTreeNodeFlags_Selected;
        if (inst->getChildren().empty())
            flags |= ImGuiTreeNodeFlags_Leaf;

        bool open = ImGui::TreeNodeEx(inst->name.c_str(), flags);

        if (ImGui::IsItemClicked())
            SelectionManager::instance().select(inst); // ★ Properties paneli buna tepki verecek

        // Sağ tık menüsü — "Insert Object" burada, reflection'dan sınıf listesi çekilir
        if (ImGui::BeginPopupContextItem()) {
            drawInsertObjectMenu(inst);
            ImGui::EndPopup();
        }

        if (open) {
            for (auto& child : inst->getChildren())
                drawInstanceNode(child); // Recursive — ağaç yapısı doğal olarak yansıyor
            ImGui::TreePop();
        }
    }

    void drawInsertObjectMenu(const std::shared_ptr<Instance>& parent) {
        if (ImGui::BeginMenu("Insert Object")) {
            // ★ Kritik: Bu liste hardcoded DEĞİL, TypeRegistry'den geliyor
            for (auto& className : TypeRegistry::instance().getAllInsertableClasses()) {
                if (ImGui::MenuItem(className.c_str())) {
                    auto newInst = createInstance(className); // Faz 1'deki generic factory
                    UndoStack::instance().pushCreateCommand(newInst, parent); // ★ Bölüm E
                }
            }
            ImGui::EndMenu();
        }
    }
};
```

**Bu, Faz 1'in ödemesini yaptığımız yer:** "Insert Object" menüsü hiçbir zaman elle güncellenmiyor. Yeni bir `SpotLight` sınıfı `ClassBuilder<SpotLight>("SpotLight")` ile kayıt edildiği anda, bu menüde otomatik beliriyor.

### C.2 Properties — reflection'ı otomatik forma çeviren sistem

```cpp
// Editor/UI/PropertiesPanel.h

class PropertiesPanel {
public:
    void draw() {
        ImGui::Begin("Properties");

        auto selected = SelectionManager::instance().getSelected();
        if (!selected) { ImGui::Text("Nothing selected"); ImGui::End(); return; }

        auto* classDesc = TypeRegistry::instance().find(getClassName(selected));

        // Kategoriye göre grupla (Faz 1.5'te eklenen "category" alanı burada işe yarıyor)
        std::map<std::string, std::vector<const PropertyDescriptor*>> byCategory;
        collectAllProperties(classDesc, byCategory); // Kalıtım zincirini de tarar

        for (auto& [category, props] : byCategory) {
            if (ImGui::CollapsingHeader(category.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto* prop : props)
                    drawPropertyEditor(selected, prop); // ★ Aşağıda detaylandırılıyor
            }
        }

        ImGui::End();
    }

private:
    void drawPropertyEditor(const std::shared_ptr<Instance>& inst, const PropertyDescriptor* prop) {
        ImGui::BeginDisabled(prop->readOnly);

        switch (prop->kind) {
            case PropertyDescriptor::Kind::Primitive:
                drawPrimitiveEditor(inst, prop); // float -> ImGui::DragFloat, bool -> ImGui::Checkbox vb.
                break;
            case PropertyDescriptor::Kind::Enum:
                drawEnumDropdown(inst, prop); // EnumRegistry'den değer listesi çekilir
                break;
            case PropertyDescriptor::Kind::Array:
                drawArrayEditor(inst, prop); // Genişletilebilir liste UI'ı
                break;
            case PropertyDescriptor::Kind::ObjectRef:
                drawObjectRefPicker(inst, prop); // "Click to select object" tarzı bir seçici
                break;
        }

        ImGui::EndDisabled();
    }

    void drawPrimitiveEditor(const std::shared_ptr<Instance>& inst, const PropertyDescriptor* prop) {
        std::any current = prop->getter(inst.get());

        if (current.type() == typeid(float)) {
            float v = std::any_cast<float>(current);
            if (ImGui::DragFloat(prop->name.c_str(), &v, 0.1f)) {
                UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, v); // ★ Bölüm E
                prop->setter(inst.get(), v);
            }
        }
        else if (current.type() == typeid(Vector3)) {
            Vector3 v = std::any_cast<Vector3>(current);
            float arr[3] = {v.x, v.y, v.z};
            if (ImGui::DragFloat3(prop->name.c_str(), arr, 0.1f)) {
                Vector3 newVal{arr[0], arr[1], arr[2]};
                UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, newVal);
                prop->setter(inst.get(), newVal);
            }
        }
        else if (current.type() == typeid(bool)) {
            bool v = std::any_cast<bool>(current);
            if (ImGui::Checkbox(prop->name.c_str(), &v)) {
                UndoStack::instance().pushPropertyChangeCommand(inst, prop->name, current, v);
                prop->setter(inst.get(), v);
            }
        }
    }
};
```

**Bu tasarımın gücü tekrar burada görünüyor:** `PropertiesPanel` kodu, `Part`, `SpotLight`, `Sound` gibi sınıfların hiçbirini **ismen bilmiyor**. Sadece "bir sınıfın property'leri var, her birinin bir `kind`'i var" bilgisiyle çalışıyor. Faz 1'deki reflection yatırımı burada tam anlamıyla karşılığını buluyor — üç fazdır inşa ettiğimiz sistemin editördeki gerçek meyvesi bu.

---

## Bölüm D — Gizmo Sistemi (Move/Rotate/Scale)

### D.1 Hazır kütüphane kullanımı: ImGuizmo

Gizmo'yu (3D taşıma/döndürme oku) sıfırdan matematik ile yazmak zaman kaybı — bu, "tekerleği yeniden icat etme" prensibimize göre hazır alınacak bir bileşen: **ImGuizmo** (ImGui ekosisteminin standart 3D manipülasyon kütüphanesi).

```cpp
// Editor/UI/Viewport.cpp içinde, handleGizmoInput()

void ViewportPanel::handleGizmoInput() {
    auto selected = SelectionManager::instance().getSelected();
    auto part = std::dynamic_pointer_cast<Part>(selected); // Sadece PVInstance türevleri taşınabilir
    if (!part) return;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

    Matrix4 transform = Matrix4::fromPositionAndRotation(part->position, part->rotation);
    Matrix4 view = editorCamera.getViewMatrix();
    Matrix4 proj = editorCamera.getProjectionMatrix(aspectRatio);

    static ImGuizmo::OPERATION currentOp = ImGuizmo::TRANSLATE; // Toolbar'dan seçilir (W/E/R kısayolları)

    bool changed = ImGuizmo::Manipulate(
        view.data(), proj.data(), currentOp, ImGuizmo::WORLD, transform.data()
    );

    if (ImGuizmo::IsUsing()) {
        isDraggingGizmo = true;
        Vector3 newPos = transform.getTranslation();
        part->setPosition(newPos); // Faz 2'deki RenderProxy dirty mekanizması otomatik tetiklenir
    }

    // Sürükleme bittiğinde TEK bir Undo kaydı oluştur (her frame değil!)
    if (isDraggingGizmo && !ImGuizmo::IsUsing()) {
        UndoStack::instance().pushPropertyChangeCommand(
            part, "Position", dragStartPosition, part->position
        );
        isDraggingGizmo = false;
    }
}
```

**Dikkat edilmesi gereken ince nokta:** Gizmo sürüklenirken her karede `Position` değişiyor ama Undo kaydı **her karede değil, sürükleme bittiğinde bir kez** oluşturuluyor. Aksi halde Ctrl+Z'ye bir kez basmak, sürüklemenin sadece son pikselini geri alırdı — kullanıcı deneyimi açısından bu yanlış olurdu.

---

## Bölüm E — Undo/Redo Sistemi (Command Pattern)

### E.1 Tasarım deseni: Command Pattern

Her kullanıcı aksiyonu (obje oluşturma, property değiştirme, obje silme) bir `Command` nesnesine sarılıyor. Bu nesne hem "yap" (`execute`) hem "geri al" (`undo`) mantığını biliyor:

```cpp
// Editor/Undo/Command.h

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute() = 0;
    virtual void undo() = 0;
};

class PropertyChangeCommand : public ICommand {
public:
    PropertyChangeCommand(std::shared_ptr<Instance> inst, std::string propName,
                           std::any oldVal, std::any newVal)
        : instance(inst), propertyName(propName), oldValue(oldVal), newValue(newVal) {}

    void execute() override {
        auto* prop = findProperty();
        prop->setter(instance.get(), newValue);
    }
    void undo() override {
        auto* prop = findProperty();
        prop->setter(instance.get(), oldValue); // ★ Aynı reflection setter'ı, eski değerle
    }

private:
    const PropertyDescriptor* findProperty() {
        return TypeRegistry::instance().find(getClassName(instance))->findProperty(propertyName);
    }

    std::shared_ptr<Instance> instance;
    std::string propertyName;
    std::any oldValue, newValue;
};

class CreateInstanceCommand : public ICommand {
public:
    CreateInstanceCommand(std::shared_ptr<Instance> newInst, std::shared_ptr<Instance> parent)
        : instance(newInst), targetParent(parent) {}

    void execute() override { instance->setParent(targetParent); }
    void undo() override { instance->setParent(nullptr); } // DataModel ağacından çıkar, ama bellekte kalır (redo için)

private:
    std::shared_ptr<Instance> instance;
    std::shared_ptr<Instance> targetParent;
};
```

### E.2 Undo Stack

```cpp
// Editor/Undo/UndoStack.h

class UndoStack {
public:
    static UndoStack& instance() { static UndoStack s; return s; }

    void pushPropertyChangeCommand(std::shared_ptr<Instance> inst, std::string prop,
                                     std::any oldVal, std::any newVal) {
        push(std::make_unique<PropertyChangeCommand>(inst, prop, oldVal, newVal));
    }

    void pushCreateCommand(std::shared_ptr<Instance> inst, std::shared_ptr<Instance> parent) {
        auto cmd = std::make_unique<CreateInstanceCommand>(inst, parent);
        cmd->execute(); // Oluşturma komutu hemen çalıştırılır
        push(std::move(cmd));
    }

    void undo() {
        if (undoList.empty()) return;
        undoList.back()->undo();
        redoList.push_back(std::move(undoList.back()));
        undoList.pop_back();
    }

    void redo() {
        if (redoList.empty()) return;
        redoList.back()->execute();
        undoList.push_back(std::move(redoList.back()));
        redoList.pop_back();
    }

private:
    void push(std::unique_ptr<ICommand> cmd) {
        undoList.push_back(std::move(cmd));
        redoList.clear(); // Yeni bir aksiyon, redo geçmişini geçersiz kılar (standart davranış)
    }

    std::vector<std::unique_ptr<ICommand>> undoList;
    std::vector<std::unique_ptr<ICommand>> redoList;
};
```

**Neden `std::any` ile eski/yeni değer saklamak sorunsuz çalışıyor:** Çünkü `PropertyDescriptor::setter`, Faz 1'de zaten `std::any` alacak şekilde tasarlanmıştı. Undo/Redo sistemi, reflection sisteminin **üçüncü** bağımsız tüketicisi oldu (ilk ikisi: Properties panel ve Luau binding). Bu, Faz 1'e yatırılan zamanın ne kadar isabetli olduğunu gösteren somut bir kanıt.

---

## Bölüm F — Klavye Kısayolları ve Editör Durumu

```cpp
// Editor/EditorApp.cpp — ana güncelleme döngüsünde

void EditorApp::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) UndoStack::instance().undo();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) UndoStack::instance().redo();
    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOp = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOp = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOp = ImGuizmo::SCALE;
    if (ImGui::IsKeyPressed(ImGuiKey_Delete)) deleteSelectedInstance();
}
```

### F.1 Play/Stop modu — Editör ve Runtime'ın aynı process'te olmasının pratik faydası

Faz 0'da verdiğimiz "editör ve motor aynı executable" kararının gerçek meyvesi burada:

```cpp
void EditorApp::onPlayButtonPressed() {
    sceneSnapshot = serializeEntireDataModel(); // Faz 1'deki serialization ile TÜM sahneyi yedekle
    editorState = EditorState::Playing;
    ScriptScheduler::instance().startAllScripts(); // Script'ler şimdi çalışmaya başlar
}

void EditorApp::onStopButtonPressed() {
    ScriptScheduler::instance().stopAllScripts();
    deserializeDataModel(sceneSnapshot); // ★ Play sırasında yapılan TÜM değişiklikler geri alınır
    editorState = EditorState::Editing;
}
```

Bu, Roblox Studio'daki "Play" deneyiminin birebir aynısı: Play'e basınca scriptler çalışır, objeler hareket eder; Stop'a basınca her şey Play öncesi haline dönüyor. Bunu ayrı bir process olarak tasarlasaydık (Gemini'nin Tauri önerisindeki gibi), bu snapshot/restore mekanizması network üzerinden senkronize edilmesi gereken çok daha karmaşık bir problem olurdu.

---

## Bölüm G — Faz 4 "Definition of Done" Kontrol Listesi

- [ ] ImGui + bgfx entegrasyonu çalışıyor, paneller sürüklenip yeniden konumlandırılabiliyor (DockSpace)
- [ ] Viewport, sahneyi render-to-texture ile gösteriyor; panel yeniden boyutlandırıldığında texture da yeniden boyutlanıyor
- [ ] Explorer paneli DataModel ağacını doğru gösteriyor, tıklanınca seçim değişiyor
- [ ] "Insert Object" menüsü **TypeRegistry'den dinamik olarak** dolduruluyor — yeni bir sınıf eklendiğinde kod değişmeden menüde beliriyor
- [ ] Properties paneli, seçili objenin tüm property'lerini kind'lerine göre doğru editör tipiyle (drag float, checkbox, dropdown vb.) gösteriyor
- [ ] Properties panelinde yapılan bir değişiklik Undo/Redo stack'ine ekleniyor ve Ctrl+Z ile geri alınabiliyor
- [ ] ImGuizmo ile obje taşıma/döndürme/ölçekleme çalışıyor, sürükleme bitince **tek bir** Undo kaydı oluşuyor (her frame değil)
- [ ] Play/Stop döngüsü çalışıyor — Play'de script'ler çalışıyor, Stop'ta sahne Play öncesi haline dönüyor
- [ ] Delete tuşu seçili objeyi siliyor ve bu da Undo edilebiliyor

---

## Sonraki Adım Önerisi

Faz 4'ün tamamlanmasıyla proje resmen **MVP noktasına** ulaşıyor — mouse ile sahne kurup, Luau ile script yazıp, Play ile test edebiliyorsun. Buradan üç yön mantıklı:

1. **Faz 5 — Fizik (Jolt entegrasyonu):** Şu ana kadar objeler görsel olarak var ama yerçekimi/çarpışma yok. `Touched` event'inin gerçekten anlam kazanması için bu şart.
2. **Faz 3'teki script timeout konusu:** Editörde artık sonsuz döngülü bir script yazan kullanıcı editörü kilitleyebilir — bu artık pratik bir sorun haline geldi, ele alınması gerekebilir.
3. **Asset Browser / dosya sistemi entegrasyonu:** Şu an sadece kod ile obje oluşturuluyor; mesh/texture import akışının editöre bağlanması.

Hangisiyle devam edelim?
