// scene_manager.hpp
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

        // ── Chargement / Sauvegarde ───────────────────────────────────────
        void load(const std::string& path);
        void save(const std::string& path);

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
        std::vector<std::string> m_sceneList; // chemins des scènes connues
    };
}