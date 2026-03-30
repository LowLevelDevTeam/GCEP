#pragma once

/// @file component_drawers.hpp
/// @brief Free @c drawField overloads for all engine types, used by @c InspectorRegistry.
///
/// Each overload renders a labelled Dear ImGui widget appropriate for the given
/// type. They are designed to be called from reflection-generated inspector code;
/// the @c InspectorRegistry selects the correct overload at compile time via
/// template argument deduction.
///
/// @par Usage
/// @code
/// float speed = 5.0f;
/// gcep::editor::drawField(speed, "Speed");
///
/// Vector3<float> pos = { 0.f, 1.f, 0.f };
/// gcep::editor::drawField(pos, "Position");
/// @endcode
///
/// @note All functions must be called from within an active Dear ImGui frame,
///       i.e. between @c ImGui::NewFrame() and @c ImGui::Render().
/// @authors Morgane Prevost, Dylan Hollemaert, Clément Bobeda, Najim Bakkali, Leo Grognet
/// @version 1.0
/// @date 2026-02-17

// Internals
#include <Engine/Core/Maths/math_includes.hpp>

// Externals
#include <imgui.h>

// STL
#include <string>

namespace gcep::editor
{
    // ── Primitives ────────────────────────────────────────────────────────────

    /// @brief Renders a drag-float widget for a scalar @c float field.
    ///
    /// @param v      Reference to the value to display and edit.
    /// @param label  Widget label shown to the left of the control.
    void drawField(float& v,              const char* label);

    /// @brief Renders a drag-int widget for a scalar @c int field.
    ///
    /// @param v      Reference to the value to display and edit.
    /// @param label  Widget label shown to the left of the control.
    void drawField(int& v,                const char* label);

    /// @brief Renders a checkbox widget for a @c bool field.
    ///
    /// @param v      Reference to the value to display and edit.
    /// @param label  Widget label shown to the right of the checkbox.
    void drawField(bool& v,               const char* label);

    /// @brief Renders a single-line text input widget for a @c std::string field.
    ///
    /// @param v      Reference to the string to display and edit.
    /// @param label  Widget label shown to the left of the input box.
    void drawField(std::string& v,        const char* label);

    // ── Math types ────────────────────────────────────────────────────────────

    /// @brief Renders two drag-float inputs for a 2-component float vector.
    ///
    /// @param v      Reference to the vector to display and edit.
    /// @param label  Widget label shown to the left of the controls.
    void drawField(Vector2<float>& v,     const char* label);

    /// @brief Renders three drag-float inputs (XYZ) for a 3-component float vector.
    ///
    /// @param v      Reference to the vector to display and edit.
    /// @param label  Widget label shown to the left of the controls.
    void drawField(Vector3<float>& v,     const char* label);

    /// @brief Renders four drag-float inputs (XYZW) for a 4-component float vector.
    ///
    /// Doubles as a colour picker representation when the field semantics call for it.
    ///
    /// @param v      Reference to the vector to display and edit.
    /// @param label  Widget label shown to the left of the controls.
    void drawField(Vector4<float>& v,     const char* label);

    /// @brief Renders a read-only 3×3 matrix display.
    ///
    /// The matrix is shown as three rows of three read-only drag-float widgets.
    /// Editing is intentionally disabled to avoid misuse of raw matrix data.
    ///
    /// @param v      Const reference to the matrix to display.
    /// @param label  Section label shown above the matrix rows.
    void drawField(const Mat3<float>& v,  const char* label);

    /// @brief Renders a read-only 4×4 matrix display.
    ///
    /// The matrix is shown as four rows of four read-only drag-float widgets.
    ///
    /// @param v      Const reference to the matrix to display.
    /// @param label  Section label shown above the matrix rows.
    void drawField(const Mat4<float>& v,  const char* label);

    /// @brief Renders four drag-float inputs (XYZW) for a quaternion.
    ///
    /// Values are displayed in raw quaternion form (not converted to Euler angles).
    ///
    /// @param v      Reference to the quaternion to display and edit.
    /// @param label  Widget label shown to the left of the controls.
    void drawField(Quaternion& v,         const char* label);

} // namespace gcep::editor
