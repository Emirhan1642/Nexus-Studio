# Aşama 4: Editor Arayüzü (ImGui, Viewport, Explorer, Properties)

Bu aşamada, motorumuzun gerçek bir oyun motoru (Roblox Studio) hissi vermesini sağlayacak olan **Editor Arayüzünü** (UI) inşa edeceğiz. Önceki fazlarda geliştirdiğimiz sistemler (Reflection, Renderer, Scripting) artık bu arayüz üzerinden görsel olarak kontrol edilecek.

## User Review Required

> [!IMPORTANT]
> - **ImGui & bgfx Entegrasyonu**: Arayüz için standart `dear-imgui` kütüphanesini kullanacağız. bgfx ile ImGui'yi bağlamak için bgfx'in `example-common` altyapısını veya kendi yazacağımız `imgui_impl_bgfx` köprüsünü kullanacağız. ImGui Immediate-Mode çalıştığı için her karede (frame) UI baştan çizilecek.
> - **ImGuizmo**: 3D sahnede objeleri fare ile taşıyıp, döndürüp, ölçeklemek (Move/Rotate/Scale) için `ImGuizmo` kütüphanesini ekleyeceğim.
> - **Undo/Redo (Command Pattern)**: Editörde yapılan her işlemin Ctrl+Z ile geri alınabilmesi için (Property değiştirme, obje oluşturma vb.) bir komut (Command) yığını sistemi kurulacak.

## Open Questions

> [!NOTE]
> 1. Editör pencerelerinin yerleşimini (layout) "Docking" özelliği ile sürükle-bırak olarak tasarlayacağım. Başlangıçta paneller (Explorer sağda, Properties altta, Viewport ortada) varsayılan bir düzende mi gelsin yoksa ImGui'nin `.ini` dosyasından son durumu mu hatırlasın? (Her ikisini de yapacağım: ilk açılışta varsayılan, sonra `.ini` üzerinden hatırlama).
> 2. `Play` ve `Stop` mekanizmasını bu faza dahil edelim mi? (Play'e basınca scriptler çalışsın, Stop'a basınca sahne eski haline dönsün). Evet ise, tüm `DataModel`'in serileştirilmesi (Serialization) altyapısının basit bir kopyalama (Clone) versiyonu gerekecek. Şimdilik "Edit/Play" durumları ekleyip Serialization işini çok basit tutmayı planlıyorum.

## Proposed Changes

### 1. Editor UI Altyapısı (ImGuiLayer)
ImGui context'inin oluşturulması ve bgfx ile köprülenmesi.

#### [NEW] Editor/UI/ImGuiLayer.h & .cpp
- `beginFrame()` ve `endFrame()` metodları ile ImGui döngüsünün kontrolü.
- `DockSpace` oluşturarak panellerin yerleştirilebilir (sürüklenebilir) olmasını sağlama.

### 2. Viewport Paneli (Render-to-Texture)
3D sahnenin doğrudan tam ekrana değil, bir pencere içine çizilmesi.

#### [NEW] Editor/UI/ViewportPanel.h & .cpp
- bgfx'te `bgfx::createFrameBuffer` ile bir hedef doku oluşturulacak.
- `RendererSystem`, sahneyi bu frame buffer'a çizecek.
- ImGui içinde `ImGui::Image` kullanılarak bu frame buffer ekranda gösterilecek.
- Kamera kontrolleri (sağ tık + WASD) sadece fare bu panelin üzerindeyken çalışacak.

### 3. Explorer ve Properties Paneli
DataModel ve Reflection sisteminin görselleştiği yer.

#### [NEW] Editor/UI/SelectionManager.h & .cpp
- Sahnede o an hangi objenin seçili olduğunu tutan Singleton sınıf.
#### [NEW] Editor/UI/ExplorerPanel.h & .cpp
- `DataModel::instance()` üzerinden başlayarak tüm ağacı tarayıp `ImGui::TreeNode` çizecek.
- Sağ tık menüsü ile "Insert Object" yapılacak (TypeRegistry'den sınıflar dinamik çekilecek).
#### [NEW] Editor/UI/PropertiesPanel.h & .cpp
- Seçili objenin `TypeRegistry`'den `PropertyDescriptor`'ları okunacak.
- Float, Vector3, Bool gibi tipler için uygun ImGui input'ları çizilecek. Değişiklikler anında reflection `setter`'ı ile objeye uygulanacak.

### 4. Undo/Redo Sistemi
Command Pattern ile işlemleri kaydetme.

#### [NEW] Editor/Undo/Command.h & UndoStack.h
- `ICommand` arayüzü ve bunu miras alan `PropertyChangeCommand`, `CreateInstanceCommand`.
- Ctrl+Z ve Ctrl+Y kısayolları `UndoStack`'e bağlanacak.

### 5. ImGuizmo Entegrasyonu
3D obje manipülasyonu.

#### [MODIFY] Editor/CMakeLists.txt
- ImGuizmo bağımlılığının projeye eklenmesi.
#### [MODIFY] Editor/UI/ViewportPanel.cpp
- Seçili objenin `Matrix4` verisini alıp `ImGuizmo::Manipulate` üzerinden taşınabilmesinin sağlanması.

## Verification Plan

### Automated Tests
- Gerekirse UndoStack için basit C++ unit testleri yazılarak (property değişiminin geri alınması) doğrulanacak.

### Manual Verification
1. `NexusStudioEditor` açıldığında karşımıza boş (veya varsayılan küplerin olduğu) bir sahne, sağda Explorer, altta Properties penceresi gelmeli.
2. Explorer'dan Workspace sağ tıklanıp yeni bir `Part` veya `Script` eklenebilmeli.
3. Eklenen `Part` seçildiğinde Properties panelinde `Position` ve `Size` gibi özellikleri görünmeli. Rakamlar değiştirildiğinde 3D Viewport'ta anında güncellenmeli.
4. Ctrl+Z tuşuna basıldığında özellik değişikliği eski haline dönmeli.
5. `ImGuizmo` ile Viewport üzerinde çıkan renkli oklar (Gizmo) ile obje sürüklenebilmeli.
