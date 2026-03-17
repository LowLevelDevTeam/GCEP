#include "component_drawers.hpp"

#include <cstring>

namespace gcep::editor
{
    void drawField(float& v, const char* label)
    {
        ImGui::DragFloat(label, &v, 0.1f);
    }

    void drawField(int& v, const char* label)
    {
        ImGui::DragInt(label, &v);
    }

    void drawField(bool& v, const char* label)
    {
        ImGui::Checkbox(label, &v);
    }

    void drawField(std::string& v, const char* label)
    {
        char buffer[256];
        strncpy(buffer, v.c_str(), sizeof(buffer));
        if (ImGui::InputText(label, buffer, sizeof(buffer)))
            v = std::string(buffer);
    }

    void drawField(Vector2<float>& v, const char* label)
    {
        float data[2] = { v.x, v.y };
        if (ImGui::DragFloat2(label, data, 0.1f))
        {
            v.x = data[0];
            v.y = data[1];
        }
    }

    void drawField(Vector3<float>& v, const char* label)
    {
        float data[3] = { v.x, v.y, v.z };
        if (ImGui::DragFloat3(label, data, 0.1f))
        {
            v.x = data[0];
            v.y = data[1];
            v.z = data[2];
        }
    }

    void drawField(Vector4<float>& v, const char* label)
    {
        float data[4] = { v.x, v.y, v.z, v.w };
        if (ImGui::DragFloat4(label, data, 0.1f))
        {
            v.x = data[0];
            v.y = data[1];
            v.z = data[2];
            v.w = data[3];
        }
    }

    void drawField(const Mat3<float>& v, const char* label)
    {
        ImGui::SeparatorText(label);
        ImGui::BeginDisabled();
        float row0[3] = { v.m[0][0], v.m[0][1], v.m[0][2] };
        float row1[3] = { v.m[1][0], v.m[1][1], v.m[1][2] };
        float row2[3] = { v.m[2][0], v.m[2][1], v.m[2][2] };
        ImGui::DragFloat3("##mat3_r0", row0);
        ImGui::DragFloat3("##mat3_r1", row1);
        ImGui::DragFloat3("##mat3_r2", row2);
        ImGui::EndDisabled();
    }

    void drawField(const Mat4<float>& v, const char* label)
    {
        ImGui::SeparatorText(label);
        ImGui::BeginDisabled();
        float row0[4] = { v.m[0][0], v.m[0][1], v.m[0][2], v.m[0][3] };
        float row1[4] = { v.m[1][0], v.m[1][1], v.m[1][2], v.m[1][3] };
        float row2[4] = { v.m[2][0], v.m[2][1], v.m[2][2], v.m[2][3] };
        float row3[4] = { v.m[3][0], v.m[3][1], v.m[3][2], v.m[3][3] };
        ImGui::DragFloat4("##mat4_r0", row0);
        ImGui::DragFloat4("##mat4_r1", row1);
        ImGui::DragFloat4("##mat4_r2", row2);
        ImGui::DragFloat4("##mat4_r3", row3);
        ImGui::EndDisabled();
    }

    void drawField(Quaternion& v, const char* label)
    {
        float data[4] = { v.w, v.x, v.y, v.z };
        if (ImGui::DragFloat4(label, data, 0.01f, -1.0f, 1.0f))
        {
            v.w = data[0];
            v.x = data[1];
            v.y = data[2];
            v.z = data[3];
            v.Normalize();
        }
    }

} // namespace gcep::editor
