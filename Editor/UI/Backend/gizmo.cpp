#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>

// Fix for ImGui 1.92.8: AddPolyline changed signature (thickness and flags swapped).
// gizmo.inl uses the old signature: (points, count, col, closed, thickness).
#define AddPolyline(pts, num, col, closed, thick) AddPolyline(pts, num, col, thick, (closed) ? ImDrawFlags_Closed : 0)

#include <widgets/gizmo.inl>
