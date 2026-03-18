#include "camera_manager.hpp"
#include <Scene/header/scene_manager.hpp>
#include <Maths/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

namespace gcep::engine
{
    void CameraManager::update(editor::EditorContext& ctx, float /*deltaTime*/)
    {
        // Snap camera to entity with CameraComponent marked as isMainCamera
        auto& registry = SLS::SceneManager::instance().current().getRegistry();
        for (auto entity : registry.view<ECS::CameraComponent, ECS::Transform>())
        {
            auto& cameraComp = registry.getComponent<ECS::CameraComponent>(entity);
            auto& transform  = registry.getComponent<ECS::Transform>(entity);

            if (!cameraComp.isMainCamera) continue;

            ctx.camera->m_position.x = transform.position.x;
            ctx.camera->m_position.y = transform.position.y;
            ctx.camera->m_position.z = transform.position.z;

            // Calculate forward direction from rotation quaternion
            glm::quat rot = glm::quat(transform.rotation.w, transform.rotation.x,
                                      transform.rotation.y, transform.rotation.z);
            glm::vec3 forward = glm::normalize(rot * glm::vec3(-1.f, 0.f, 0.f)); // default front is (-1, 0, 0)

            ctx.camera->m_front = forward;
            ctx.camera->m_pitch = glm::degrees(std::asin(glm::clamp(forward.z, -1.f, 1.f)));
            ctx.camera->m_yaw   = glm::degrees(std::atan2(forward.y, forward.x));
        }
    }
} // namespace gcep::engine
