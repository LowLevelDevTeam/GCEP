#pragma once

// Internals
#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>

namespace gcep::panel
{
    class SceneSettingsPanel : public IPanel
    {
    public:
        void draw() override;
    };

} // namespace gcep::panel
