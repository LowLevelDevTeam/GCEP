#include "script_ecs_bridge.hpp"

// Internals
#include "Engine/Core/ECS/headers/registry.hpp"
#include "Engine/Core/ECS/Components/components.hpp"
#include <Maths/quaternion.hpp>
#include <Maths/vector3.hpp>

namespace gcep
{
    using R = ECS::Registry;

    static R *reg(void *p)
    {
        return static_cast<R *>(p);
    }

    // Component type IDs
    enum class SType : unsigned int
    {
        Transform = 0,
        WorldTransform = 1,
        Physics = 2,
        Camera = 3,
        PointLight = 4,
        SpotLight = 5,
        Hierarchy = 6,
    };

    // =========================================================================
    // has / get / add / remove
    // =========================================================================

    static bool impl_has(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:      return reg(r)->hasComponent<ECS::Transform>(id);
            case SType::WorldTransform: return reg(r)->hasComponent<ECS::WorldTransform>(id);
            case SType::Physics:        return reg(r)->hasComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:         return reg(r)->hasComponent<ECS::CameraComponent>(id);
            case SType::PointLight:     return reg(r)->hasComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:      return reg(r)->hasComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:      return reg(r)->hasComponent<ECS::HierarchyComponent>(id);
            default:                    return false;
        }
    }

    static ComponentHandle impl_get(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:      return &reg(r)->getComponent<ECS::Transform>(id);
            case SType::WorldTransform: return &reg(r)->getComponent<ECS::WorldTransform>(id);
            case SType::Physics:        return &reg(r)->getComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:         return &reg(r)->getComponent<ECS::CameraComponent>(id);
            case SType::PointLight:     return &reg(r)->getComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:      return &reg(r)->getComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:      return &reg(r)->getComponent<ECS::HierarchyComponent>(id);
            default:                    return nullptr;
        }
    }

    static ComponentHandle impl_add(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:      return &reg(r)->addComponent<ECS::Transform>(id);
            case SType::WorldTransform: return &reg(r)->addComponent<ECS::WorldTransform>(id);
            case SType::Physics:        return &reg(r)->addComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:         return &reg(r)->addComponent<ECS::CameraComponent>(id);
            case SType::PointLight:     return &reg(r)->addComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:      return &reg(r)->addComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:      return &reg(r)->addComponent<ECS::HierarchyComponent>(id);
            default:                    return nullptr;
        }
    }

    static void impl_remove(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:      reg(r)->removeComponent<ECS::Transform>(id);      break;
            case SType::WorldTransform: reg(r)->removeComponent<ECS::WorldTransform>(id); break;
            case SType::Physics:        reg(r)->removeComponent<ECS::PhysicsComponent>(id); break;
            case SType::Camera:         reg(r)->removeComponent<ECS::CameraComponent>(id); break;
            case SType::PointLight:     reg(r)->removeComponent<ECS::PointLightComponent>(id); break;
            case SType::SpotLight:      reg(r)->removeComponent<ECS::SpotLightComponent>(id); break;
            case SType::Hierarchy:      reg(r)->removeComponent<ECS::HierarchyComponent>(id); break;
            default: break;
        }
    }

    // =========================================================================
    // Entity lifetime
    // =========================================================================

    static EntityID impl_createEntity(void *r)
    {
        return reg(r)->createEntity();
    }

    static void impl_destroyEntity(void *r, EntityID id)
    {
        reg(r)->destroyEntity(id);
        reg(r)->update();
    }

    // =========================================================================
    // Hierarchy
    // =========================================================================

    static EntityID impl_getParent(void *r, EntityID id)
    {
        if (!reg(r)->hasComponent<ECS::HierarchyComponent>(id))
            return GCEP_INVALID_ENTITY;
        return reg(r)->getComponent<ECS::HierarchyComponent>(id).parent;
    }

    static EntityID impl_getFirstChild(void *r, EntityID id)
    {
        if (!reg(r)->hasComponent<ECS::HierarchyComponent>(id))
            return GCEP_INVALID_ENTITY;
        return reg(r)->getComponent<ECS::HierarchyComponent>(id).firstChild;
    }

    static EntityID impl_getNextSibling(void *r, EntityID id)
    {
        if (!reg(r)->hasComponent<ECS::HierarchyComponent>(id))
            return GCEP_INVALID_ENTITY;
        return reg(r)->getComponent<ECS::HierarchyComponent>(id).nextSibling;
    }

    // =========================================================================
    // String helpers
    // =========================================================================

    static int impl_getMeshPath(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::MeshComponent>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::MeshComponent>(id).filePath;
        int n = std::min((int)s.size(), len - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
        return n;
    }

    static int impl_getTexturePath(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::MeshComponent>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::MeshComponent>(id).texturePath;
        int n = std::min((int)s.size(), len - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
        return n;
    }

    static int impl_getTag(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::Tag>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::Tag>(id).name;
        int n = std::min((int)s.size(), len - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
        return n;
    }

    static void impl_setTag(void *r, EntityID id, const char *tag)
    {
        if (!reg(r)->hasComponent<ECS::Tag>(id))
            reg(r)->addComponent<ECS::Tag>(id);
        reg(r)->getComponent<ECS::Tag>(id).name = tag;
    }

    // =========================================================================
    // Physics runtime
    // =========================================================================

    static void impl_getLinearVelocity(void *r, EntityID id, float *out)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
        {
            out[0] = out[1] = out[2] = 0.f;
            return;
        }
        const auto &v = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).linearVelocity;
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
    }

    static void impl_setLinearVelocity(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &v = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).linearVelocity;
        v.x = x; v.y = y; v.z = z;
    }

    static void impl_addForce(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &f = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).force;
        f.x += x; f.y += y; f.z += z;
    }

    static void impl_addImpulse(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &f = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).impulse;
        f.x += x; f.y += y; f.z += z;
    }

    // =========================================================================
    // Rotation helpers
    // =========================================================================

    /// Normalise q, write it into the Transform, and refresh the Euler display-cache.
    static void syncRotation(ECS::Transform &t, const Quaternion &q)
    {
        const Quaternion n = q.Normalized();
        t.rotation = n;

        const Vector3<float> e = n.ToEuler(); // pitch, yaw, roll
        t.eulerRadians = { e.x, e.y, e.z };
    }

    static void impl_setRotationEuler(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::Transform>(id)) return;
        auto &t = reg(r)->getComponent<ECS::Transform>(id);
        syncRotation(t, Quaternion::FromEuler({ x, y, z }));
    }

    static void impl_rotateEuler(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::Transform>(id)) return;
        auto &t = reg(r)->getComponent<ECS::Transform>(id);

        const Quaternion current = t.rotation;
        const Quaternion delta   = Quaternion::FromEuler({ x, y, z });
        // Pre-multiply: delta is applied on top of the current rotation.
        syncRotation(t, delta * current);
    }

    static void impl_getRotationQuat(void *r, EntityID id, float *out)
    {
        if (!reg(r)->hasComponent<ECS::Transform>(id))
        {
            out[0] = 1.f; out[1] = out[2] = out[3] = 0.f; // identity
            return;
        }
        const Quaternion &q = reg(r)->getComponent<ECS::Transform>(id).rotation;
        out[0] = q.w; out[1] = q.x; out[2] = q.y; out[3] = q.z;
    }

    static void impl_getRotationEuler(void *r, EntityID id, float *out)
    {
        if (!reg(r)->hasComponent<ECS::Transform>(id))
        {
            out[0] = out[1] = out[2] = 0.f;
            return;
        }
        const Vector3<float> e = reg(r)->getComponent<ECS::Transform>(id).rotation.Normalized().ToEuler();
        out[0] = e.x; out[1] = e.y; out[2] = e.z;
    }

    // =========================================================================
    // ScriptECSAPI singleton
    // =========================================================================

    static constexpr ScriptECSAPI ECS_API
    {
        impl_has, impl_get, impl_add, impl_remove,
        impl_createEntity, impl_destroyEntity,
        impl_getParent, impl_getFirstChild, impl_getNextSibling,
        impl_getMeshPath, impl_getTexturePath,
        impl_getTag, impl_setTag,
        impl_getLinearVelocity, impl_setLinearVelocity, impl_addForce, impl_addImpulse,
        impl_setRotationEuler, impl_rotateEuler, impl_getRotationQuat, impl_getRotationEuler
    };

    const ScriptECSAPI *getScriptECSAPI()
    {
        return &ECS_API;
    }

    // =========================================================================
    // ScriptCameraAPI — implementation
    // =========================================================================

    /// Returns a pointer to the CameraComponent, or nullptr if absent.
    static ECS::CameraComponent *cam(void *r, EntityID id)
    {
        if (!reg(r)->hasComponent<ECS::CameraComponent>(id))
            return nullptr;
        return &reg(r)->getComponent<ECS::CameraComponent>(id);
    }

    // ── Projection ────────────────────────────────────────────────────────

    static float impl_cam_getFovY(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->fovYDeg : 60.f;
    }

    static void impl_cam_setFovY(void *r, EntityID id, float fovYDeg)
    {
        auto *c = cam(r, id);
        if (!c) return;
        // Clamp to a sane range — 0° or 180° would produce a degenerate matrix.
        if (fovYDeg <= 0.f)   fovYDeg = 0.01f;
        if (fovYDeg >= 180.f) fovYDeg = 179.99f;
        c->fovYDeg = fovYDeg;
    }

    static float impl_cam_getNearZ(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->nearZ : 0.01f;
    }

    static void impl_cam_setNearZ(void *r, EntityID id, float nearZ)
    {
        auto *c = cam(r, id);
        if (!c) return;
        if (nearZ <= 0.f) nearZ = 0.0001f; // must be positive
        c->nearZ = nearZ;
    }

    static float impl_cam_getFarZ(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->farZ : 1000.f;
    }

    static void impl_cam_setFarZ(void *r, EntityID id, float farZ)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->farZ = farZ;
    }

    static bool impl_cam_isMain(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->isMainCamera : false;
    }

    static void impl_cam_setMain(void *r, EntityID id, bool isMain)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->isMainCamera = isMain;
    }

    // ── Position ──────────────────────────────────────────────────────────

    static void impl_cam_getPosition(void *r, EntityID id, float *out)
    {
        const auto *c = cam(r, id);
        if (!c) { out[0] = out[1] = out[2] = 0.f; return; }
        out[0] = c->position.x;
        out[1] = c->position.y;
        out[2] = c->position.z;
    }

    static void impl_cam_setPosition(void *r, EntityID id, float x, float y, float z)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->position = { x, y, z };
    }

    static void impl_cam_translate(void *r, EntityID id, float dx, float dy, float dz)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->position.x += dx;
        c->position.y += dy;
        c->position.z += dz;
    }

    // ── Orientation ───────────────────────────────────────────────────────

    static constexpr float PITCH_LIMIT = 89.f;

    static float impl_cam_getPitch(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->pitch : 0.f;
    }

    static void impl_cam_setPitch(void *r, EntityID id, float pitch)
    {
        auto *c = cam(r, id);
        if (!c) return;
        if (pitch >  PITCH_LIMIT) pitch =  PITCH_LIMIT;
        if (pitch < -PITCH_LIMIT) pitch = -PITCH_LIMIT;
        c->pitch = pitch;
    }

    static void impl_cam_addPitch(void *r, EntityID id, float deltaDeg)
    {
        auto *c = cam(r, id);
        if (!c) return;
        float p = c->pitch + deltaDeg;
        if (p >  PITCH_LIMIT) p =  PITCH_LIMIT;
        if (p < -PITCH_LIMIT) p = -PITCH_LIMIT;
        c->pitch = p;
    }

    static float impl_cam_getYaw(void *r, EntityID id)
    {
        const auto *c = cam(r, id);
        return c ? c->yaw : -180.f;
    }

    static void impl_cam_setYaw(void *r, EntityID id, float yaw)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->yaw = yaw;
    }

    static void impl_cam_addYaw(void *r, EntityID id, float deltaDeg)
    {
        auto *c = cam(r, id);
        if (!c) return;
        c->yaw += deltaDeg;
    }

    // =========================================================================
    // ScriptCameraAPI singleton
    // =========================================================================

    static constexpr ScriptCameraAPI CAM_API
    {
        impl_cam_getFovY,     impl_cam_setFovY,
        impl_cam_getNearZ,    impl_cam_setNearZ,
        impl_cam_getFarZ,     impl_cam_setFarZ,
        impl_cam_isMain,      impl_cam_setMain,
        impl_cam_getPosition, impl_cam_setPosition, impl_cam_translate,
        impl_cam_getPitch,    impl_cam_setPitch,    impl_cam_addPitch,
        impl_cam_getYaw,      impl_cam_setYaw,      impl_cam_addYaw,
    };

    const ScriptCameraAPI *getScriptCameraAPI()
    {
        return &CAM_API;
    }

} // namespace gcep
