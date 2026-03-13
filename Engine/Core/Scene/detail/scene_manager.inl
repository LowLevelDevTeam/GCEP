#pragma once
#include <stdexcept>
#include <algorithm>
#include <filesystem>
#include <Engine/Core/Scene/header/scene_manager.hpp>

#include "Engine/RHI/Vulkan/VulkanRHI.hpp"

namespace gcep::SLS
{
    inline SceneManager& SceneManager::instance()
    {
        static SceneManager manager;
        return manager;
    }

    inline Scene& SceneManager::current()
    {
        if (!m_currentScene)
        {
            throw std::runtime_error("No current scene");
        }

        return *m_currentScene;
    }

    inline void SceneManager::saveScene(const std::string& path)
    {
        m_currentScene->save(path);
    }

    inline void SceneManager::loadScene(const std::string& path, rhi::vulkan::VulkanRHI* rhi)
    {
        m_currentScene = std::make_unique<Scene>();

        if (std::filesystem::exists(path))
        {
            m_currentScene->load(path, rhi);
        }
    }

    inline void SceneManager::registerScene(const std::string& path)
    {
        auto it = std::find(m_sceneList.begin(), m_sceneList.end(), path);
        if (it == m_sceneList.end())
        {
            m_sceneList.push_back(path);
        }
    }

    inline const std::vector<std::string>& SceneManager::getSceneList() const
    {
        return m_sceneList;
    }

    inline void SceneManager::update(float deltaTime)
    {
        if (!m_currentScene) return;
        m_currentScene->onUpdateRuntime(deltaTime);
    }
}
