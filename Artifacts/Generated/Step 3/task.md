# Aşama 4: Editor Arayüzü (ImGui) Görevleri

- `[ ]` **1. ImGui ve ImGuizmo Kurulumu**
  - `[ ]` `dear-imgui` ve `ImGuizmo` kütüphanelerinin CMake ile `ThirdParty` içerisine eklenmesi.
  - `[ ]` bgfx için ImGui backend (`imgui_impl_bgfx`) kodlarının projeye entegre edilmesi.

- `[ ]` **2. ImGuiLayer (Altyapı)**
  - `[ ]` `Editor/UI/ImGuiLayer.h` ve `.cpp` oluşturulması.
  - `[ ]` ImGui context'inin başlatılması ve DockSpace altyapısının kurulması (Kullanıcı İsteği: Explorer ve Properties sağda doklanmış olacak).

- `[ ]` **3. Viewport (Render-to-Texture)**
  - `[ ]` `RendererSystem`'in ekrana değil Frame Buffer'a çizecek şekilde güncellenmesi.
  - `[ ]` `ViewportPanel` oluşturularak Frame Buffer dokusunun `ImGui::Image` ile ekrana çizdirilmesi.

- `[ ]` **4. Selection ve Undo/Redo Sistemi**
  - `[ ]` Sahnede seçili objeyi tutan `SelectionManager`'ın yazılması.
  - `[ ]` `ICommand`, `PropertyChangeCommand`, `CreateInstanceCommand` ve `UndoStack` altyapısının kurulması (Ctrl+Z / Ctrl+Y).

- `[ ]` **5. Explorer ve Properties Panelleri**
  - `[ ]` `ExplorerPanel` üzerinden DataModel ağacının `ImGui::TreeNode` ile çizilmesi, "Insert Object" menüsünün eklenmesi.
  - `[ ]` `PropertiesPanel` üzerinden seçili objenin `TypeRegistry`'den alınan özelliklerinin input olarak (DragFloat, Checkbox vb.) çizdirilmesi.

- `[ ]` **6. ImGuizmo Entegrasyonu**
  - `[ ]` Seçili objenin `ImGuizmo` okları ile sahnede (Viewport üzerinde) hareket ettirilebilmesi.

- `[ ]` **7. Son Entegrasyon**
  - `[ ]` Tüm UI parçalarının `Main.cpp` veya bir `EditorApp` sınıfı içinde birleştirilerek test edilmesi.
