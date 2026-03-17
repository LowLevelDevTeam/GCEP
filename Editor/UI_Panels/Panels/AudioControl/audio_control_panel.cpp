#include "audio_control_panel.hpp"

#include <Editor/Helpers.hpp>
#include <tinyfiledialogs.h>
#include <imgui.h>

#include <filesystem>
#include <string>

namespace gcep::panel
{
    void AudioControlPanel::draw()
    {
        ImGui::Begin("Audio control");

        AudioSystem* audio = AudioSystem::getInstance();
        audio->update();

        for (int i = 0; i < static_cast<int>(m_audioSources.size()); ++i)
        {
            auto& source = m_audioSources[i];

            bool isPlaying     = source->isPlaying();
            bool isLooping     = source->isLooping();
            bool isSpatialized = source->isSpatialized();

            drawVec3Control("Audio source " + std::to_string(i), source->getPosition());

            if (ImGui::Checkbox(("Play##" + std::to_string(i)).c_str(), &isPlaying))
            {
                if (source->isPlaying()) source->pause();
                else                     source->play();
            }
            ImGui::SameLine();
            if (ImGui::Checkbox(("Loop##" + std::to_string(i)).c_str(), &isLooping))
                source->setLooping(!source->isLooping());
            ImGui::SameLine();
            if (ImGui::Checkbox(("Spatialize##" + std::to_string(i)).c_str(), &isSpatialized))
                source->setSpatialized(!source->isSpatialized());
        }

        ImGui::SeparatorText("Options");
        if (ImGui::Button("Add audio source"))
        {
            const char* filters[] = { "*.mp3", "*.wav", "*.flac" };
            const char* path = tinyfd_openFileDialog("Add an audio source",
                std::filesystem::current_path().string().c_str(), 3, filters, "Audio files", 0);
            if (path)
            {
                m_audioSources.push_back(audio->loadAudio(path));
                m_audioSources.back()->setVolume(1.f);
                m_audioSources.back()->setPitch(1.f);
            }
        }

        ImGui::End();
    }

} // namespace gcep::panel
