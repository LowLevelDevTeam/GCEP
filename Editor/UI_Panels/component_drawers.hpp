#pragma once

/// @file component_drawers.hpp
/// @brief Free drawField functions for all engine types, used by InspectorRegistry.

#include <imgui.h>
#include <Engine/Core/Maths/math_includes.hpp>

#include <string>

namespace gcep::editor
{
    // ── Primitives ────────────────────────────────────────────────────────────
    void drawField(float& v,              const char* label);
    void drawField(int& v,                const char* label);
    void drawField(bool& v,               const char* label);
    void drawField(std::string& v,        const char* label);

    // ── Math types ────────────────────────────────────────────────────────────
    void drawField(Vector2<float>& v,     const char* label);
    void drawField(Vector3<float>& v,     const char* label);
    void drawField(Vector4<float>& v,     const char* label);
    void drawField(const Mat3<float>& v,  const char* label);
    void drawField(const Mat4<float>& v,  const char* label);
    void drawField(Quaternion& v,         const char* label);

} // namespace gcep::editor
