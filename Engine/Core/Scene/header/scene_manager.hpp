#pragma once

// Internals
#include <Scene/header/scene.hpp>

// STL
#include <memory>
#include <string>
#include <vector>

namespace gcep::SLS
{
    class SceneManager
    {
    public:
        static SceneManager& instance();

        Scene& current();

        void saveScene(const std::string &path);

        void loadScene(const std::string& path, rhi::vulkan::VulkanRHI* rhi);

        void registerScene(const std::string& path);
        const std::vector<std::string>& getSceneList() const;

        void update(float deltaTime);


    private:
        SceneManager() = default;
        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        std::unique_ptr<Scene>   m_currentScene;
        std::vector<std::string> m_sceneList;
    };
} // namespace gcep::SLS

#include <Scene/detail/scene_manager.inl>
