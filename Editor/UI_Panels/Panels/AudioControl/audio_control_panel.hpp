#pragma once

#include <Editor/UI_Panels/ipanel.hpp>
#include <Engine/Audio/audio_system.hpp>
#include <Engine/Audio/audio_source.hpp>

#include <memory>
#include <vector>

namespace gcep::panel
{
    class AudioControlPanel : public IPanel
    {
    public:
        void draw() override;

    private:
        std::vector<std::shared_ptr<AudioSource>> m_audioSources;
    };

} // namespace gcep::panel
