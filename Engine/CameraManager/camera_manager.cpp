#include "camera_manager.hpp"

// Internals
#include <Scene/header/scene_manager.hpp>

// Externals
#include <glm/glm.hpp>

namespace gcep::engine
{
    void CameraManager::update(editor::EditorContext& ctx, float /*deltaTime*/)
    {
        // Snap camera to entity with CameraComponent marked as isMainCamera
        auto& registry = SLS::SceneManager::instance().current().getRegistry();
        for (auto entity : registry.view<ECS::CameraComponent>())
        {
            auto& cameraComp = registry.getComponent<ECS::CameraComponent>(entity);
            if (!cameraComp.isMainCamera) continue;

            ctx.camera->m_position.x = cameraComp.position.x;
            ctx.camera->m_position.y = cameraComp.position.y;
            ctx.camera->m_position.z = cameraComp.position.z;

            // Derive forward directly from the component's own pitch/yaw.
            const float pitchRad = glm::radians(cameraComp.pitch);
            const float yawRad   = glm::radians(cameraComp.yaw);
            glm::vec3 forward;
            forward.x = std::cos(yawRad) * std::cos(pitchRad);
            forward.y = std::sin(yawRad) * std::cos(pitchRad);
            forward.z = std::sin(pitchRad);

            ctx.camera->m_front   = glm::normalize(forward);
            ctx.camera->m_pitch   = cameraComp.pitch;
            ctx.camera->m_yaw     = cameraComp.yaw;
            ctx.camera->m_fovYDeg = cameraComp.fovYDeg;
        }
    }
} // namespace gcep::engine
