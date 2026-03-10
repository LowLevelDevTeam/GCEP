#pragma once
#include <memory>
#include <string>
#include <vector>
#include <Engine/Core/Scene/header/scene.hpp>

namespace gcep::SLS
{
    class SceneManager
    {
    public:
        static SceneManager& instance();

        // ── Scène courante ────────────────────────────────────────────────
        Scene& current();

        void saveScene(const std::string &path);

        // ── Chargement ────────────────────────────────────────────────────
        void loadScene(const std::string& path, rhi::vulkan::VulkanRHI* rhi);

        // ── Gestion des scènes connues ────────────────────────────────────
        void registerScene(const std::string& path);
        const std::vector<std::string>& getSceneList() const;

        // ── Lifecycle ─────────────────────────────────────────────────────
        void update(float deltaTime);


    private:
        SceneManager() = default;
        SceneManager(const SceneManager&)            = delete;
        SceneManager& operator=(const SceneManager&) = delete;

        std::unique_ptr<Scene>   m_currentScene;
        std::vector<std::string> m_sceneList;
    };
}

#include <Engine/Core/Scene/detail/scene_manager.inl>
