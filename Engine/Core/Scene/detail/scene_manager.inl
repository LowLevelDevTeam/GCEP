#pragma once
#include <stdexcept>
#include <algorithm>
#include <Engine/Core/Scene/header/scene_manager.hpp>

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
            throw std::runtime_error("SceneManager: no active Scene");
        return *m_currentScene;
    }

    inline void SceneManager::loadScene(const std::string& path)
    {
        m_currentScene = std::make_unique<Scene>();
        m_currentScene->load(path);
    }

    inline void SceneManager::registerScene(const std::string& path)
    {
        auto it = std::find(m_sceneList.begin(), m_sceneList.end(), path);
        if (it == m_sceneList.end())
            m_sceneList.push_back(path);
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
