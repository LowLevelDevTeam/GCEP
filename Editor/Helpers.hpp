#pragma once

#include <Externals/imgui/imgui_internal.h>
#include <filesystem>
#include <fstream>
#include "font_awesome.hpp"

namespace gcep
{
    enum class SimulationState{ STOPPED, PLAYING, PAUSED};



static bool isSpecificWindowFocused(const char* name)
{
    ImGuiWindow* window = ImGui::FindWindowByName(name);
    if (!window) return false;

    ImGuiContext& g = *GImGui;
    return (g.NavWindow == window || g.NavWindow == window->RootWindow) && window->Active;
}

inline void setDarkTheme()
{
    auto &style = ImGui::GetStyle();
    auto &colors = style.Colors;

    // geometry & spacing
    style.FramePadding = ImVec2(6, 4);
    style.CellPadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(8, 6);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 10.0f;

    // borders & rounding
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    // core palette (complete)
    colors[ImGuiCol_Text] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.09f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.96f);
    colors[ImGuiCol_Border] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);

    colors[ImGuiCol_TitleBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);

    colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.07f, 1.00f);

    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

    colors[ImGuiCol_Header] = ImVec4(0.22f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.38f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.45f, 0.65f, 1.00f);

    colors[ImGuiCol_Separator] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);

    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

    // docking
    colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.40f, 0.60f, 0.40f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    // plots
    colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.60f, 0.76f, 0.92f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.35f, 0.55f, 0.75f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.55f, 0.72f, 0.90f, 1.00f);

    // tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);

    // selection & drag/drop
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.40f, 0.60f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.40f, 0.60f, 0.80f, 0.90f);

    // navigation & modal
    colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.60f, 0.80f, 0.30f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.40f, 0.60f, 0.80f, 0.25f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.55f);
}

inline std::string getFileIcon(const std::filesystem::path& file) {
    std::string ext = file.extension().string();
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg")  return ICON_FA_FILE_IMAGE_O;
    if (ext == ".mp3" || ext == ".wav")                    return ICON_FA_FILE_AUDIO_O;
    if (ext == ".mp4" || ext == ".avi")                    return ICON_FA_FILE_VIDEO_O;
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h")     return ICON_FA_FILE_CODE_O;
    if (ext == ".zip" || ext == ".rar")                    return ICON_FA_FILE_ARCHIVE_O;
    return ICON_FA_FILE;
}

} // Namespace gcep
