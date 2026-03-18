#include "performance_panel.hpp"

#include <string>

namespace gcep::panel
{
    void PerformancePanel::draw()
    {
        const float dt  = ImGui::GetIO().DeltaTime;
        const float fps = (dt > 0.f) ? 1.f / dt : 0.f;

        m_frameTimes[m_offset] = dt * 1000.f; // ms
        m_offset = (m_offset + 1) % k_historySize;

        float sum = 0.f, minT = m_frameTimes[0], maxT = m_frameTimes[0];
        for (float t : m_frameTimes)
        {
            sum  += t;
            if (t > maxT) maxT = t;
            if (t < minT) minT = t;
        }
        m_fpsAvg = (sum > 0.f) ? 1000.f / (sum / k_historySize) : 0.f;
        m_fpsMin = (maxT > 0.001f) ? 1000.f / maxT : 0.f;
        m_fpsMax = (minT > 0.001f) ? 1000.f / minT : 0.f;

        ImGui::Begin("Performance");

        ImGui::Text("FPS     %6.1f", fps);
        ImGui::Text("Avg     %6.1f  (min %.1f  max %.1f)", m_fpsAvg, m_fpsMin, m_fpsMax);
        ImGui::Text("Frame   %.3f ms", dt * 1000.f);

        ImGui::Spacing();

        const std::string overlay = std::to_string(static_cast<int>(fps)) + " FPS";
        ImGui::PlotLines("##ft", m_frameTimes.data(), k_historySize, m_offset,
            overlay.c_str(), 0.f, 50.f, ImVec2(ImGui::GetContentRegionAvail().x, 60.f));

        ImGui::End();
    }

} // namespace gcep::panel
