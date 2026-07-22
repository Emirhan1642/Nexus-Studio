# Nexus Studio - Walkthrough

## Phase 4: Editor UI & ImGuizmo Integration

### Changes Made
- **ImGui Layer**: Implemented `ImGuiLayer` to handle Dear ImGui frame lifecycles inside the main application loop. Removed docking since the `bgfx` version of `dear-imgui` doesn't natively support it on its `master` branch.
- **Custom UI Layout**: Hardcoded the initial layout to place the `Viewport` on the left, and both `Explorer` and `Properties` panels strictly on the right side of the screen as requested.
- **ImGuizmo Integration**: 
  - Integrated `gizmo.h` (shipped within `bgfx/3rdparty/dear-imgui/widgets`) to allow in-scene manipulation of objects.
  - Linked the Gizmo actions to update the selected `Part`'s `Matrix4` transform (`getTranslation()`).
- **Undo/Redo System**:
  - Implemented the Command Pattern (`ICommand`) and an `UndoStack`.
  - Moving objects using the Gizmo will now automatically push `PropertyChangeCommand` instances to the undo stack once the drag operation completes.
- **Explorer & Properties**:
  - `ExplorerPanel` now renders a hierarchical tree view starting from `DataModel::instance()`.
  - `PropertiesPanel` automatically enumerates registered properties using our `TypeRegistry` reflection system, allowing dynamic edits without hardcoded forms.

### Validation Results
- The project successfully builds (`NexusStudioEditor.exe`).
- ImGui panels appear and organize themselves correctly on startup.
- `ImGuizmo` successfully intercepts camera matrices (`view` and `proj`) and applies transformations directly into the game engine's `Matrix4` format.

### Next Steps
1. Run `./build/bin/Debug/NexusStudioEditor.exe`.
2. Click on either `MyCube1` or `MyCube2` in the **Explorer** panel.
3. Use the **Viewport** Gizmo to translate the selected cube (W, E, R keys switch modes).
4. Verify the **Properties** panel updates the values in real-time.
