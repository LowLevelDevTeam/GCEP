#include <imgui_internal.h>

namespace gcep
{
    static bool isSpecificWindowFocused(const char* name)
    {
        ImGuiWindow* window = ImGui::FindWindowByName(name);
        if (!window) return false;

        ImGuiContext& g = *GImGui;
        return (g.NavWindow == window || g.NavWindow == window->RootWindow) && window->Active;
    }
}