#pragma once

#include <Editor/UI_Panels/ipanel.hpp>
#include <imgui.h>
#include <array>

namespace gcep::panel
{
    class PerformancePanel : public IPanel
    {
    public:
        void draw() override;

    private:
        static constexpr int k_historySize = 128;

        std::array<float, k_historySize> m_frameTimes = {};
        int   m_offset    = 0;
        float m_fpsMin    = 0.f;
        float m_fpsMax    = 0.f;
        float m_fpsAvg    = 0.f;
    };

} // namespace gcep::panel
