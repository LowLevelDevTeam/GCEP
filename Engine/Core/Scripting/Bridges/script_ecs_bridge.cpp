#include "script_ecs_bridge.hpp"

// Internals
#include "Engine/Core/ECS/headers/registry.hpp"
#include "Engine/Core/ECS/Components/components.hpp"

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

    // has / get / add / remove

    static bool impl_has(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:
                return reg(r)->hasComponent<ECS::Transform>(id);
            case SType::WorldTransform:
                return reg(r)->hasComponent<ECS::WorldTransform>(id);
            case SType::Physics:
                return reg(r)->hasComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:
                return reg(r)->hasComponent<ECS::CameraComponent>(id);
            case SType::PointLight:
                return reg(r)->hasComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:
                return reg(r)->hasComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:
                return reg(r)->hasComponent<ECS::HierarchyComponent>(id);
            default:
                return false;
        }
    }

    static ComponentHandle impl_get(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:
                return &reg(r)->getComponent<ECS::Transform>(id);
            case SType::WorldTransform:
                return &reg(r)->getComponent<ECS::WorldTransform>(id);
            case SType::Physics:
                return &reg(r)->getComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:
                return &reg(r)->getComponent<ECS::CameraComponent>(id);
            case SType::PointLight:
                return &reg(r)->getComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:
                return &reg(r)->getComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:
                return &reg(r)->getComponent<ECS::HierarchyComponent>(id);
            default:
                return nullptr;
        }
    }

    static ComponentHandle impl_add(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:
                return &reg(r)->addComponent<ECS::Transform>(id);
            case SType::WorldTransform:
                return &reg(r)->addComponent<ECS::WorldTransform>(id);
            case SType::Physics:
                return &reg(r)->addComponent<ECS::PhysicsComponent>(id);
            case SType::Camera:
                return &reg(r)->addComponent<ECS::CameraComponent>(id);
            case SType::PointLight:
                return &reg(r)->addComponent<ECS::PointLightComponent>(id);
            case SType::SpotLight:
                return &reg(r)->addComponent<ECS::SpotLightComponent>(id);
            case SType::Hierarchy:
                return &reg(r)->addComponent<ECS::HierarchyComponent>(id);
            default:
                return nullptr;
        }
    }

    static void impl_remove(void *r, EntityID id, ComponentTypeID t)
    {
        switch (static_cast<SType>(t))
        {
            case SType::Transform:
                reg(r)->removeComponent<ECS::Transform>(id);
                break;
            case SType::WorldTransform:
                reg(r)->removeComponent<ECS::WorldTransform>(id);
                break;
            case SType::Physics:
                reg(r)->removeComponent<ECS::PhysicsComponent>(id);
                break;
            case SType::Camera:
                reg(r)->removeComponent<ECS::CameraComponent>(id);
                break;
            case SType::PointLight:
                reg(r)->removeComponent<ECS::PointLightComponent>(id);
                break;
            case SType::SpotLight:
                reg(r)->removeComponent<ECS::SpotLightComponent>(id);
                break;
            case SType::Hierarchy:
                reg(r)->removeComponent<ECS::HierarchyComponent>(id);
                break;
            default:
                break;
        }
    }

    // Entity lifetime

    static EntityID impl_createEntity(void *r)
    {
        return reg(r)->createEntity();
    }

    static void impl_destroyEntity(void *r, EntityID id)
    {
        reg(r)->destroyEntity(id);
        reg(r)->update();
    }

    // Hierarchy

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

    // String helpers

    static int impl_getMeshPath(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::MeshComponent>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::MeshComponent>(id).filePath;
        int n = std::min((int) s.size(), len - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
        return n;
    }

    static int impl_getTexturePath(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::MeshComponent>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::MeshComponent>(id).texturePath;
        int n = std::min((int) s.size(), len - 1);
        std::memcpy(buf, s.data(), n);
        buf[n] = '\0';
        return n;
    }

    static int impl_getTag(void *r, EntityID id, char *buf, int len)
    {
        if (!reg(r)->hasComponent<ECS::Tag>(id))
            return 0;
        const auto &s = reg(r)->getComponent<ECS::Tag>(id).name;
        int n = std::min((int) s.size(), len - 1);
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

    // Physics runtime

    static void impl_getLinearVelocity(void *r, EntityID id, float *out)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
        {
            out[0] = out[1] = out[2] = 0.f;
            return;
        }
        auto &v = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).linearVelocity;
        out[0] = v.x;
        out[1] = v.y;
        out[2] = v.z;
    }

    static void impl_setLinearVelocity(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &v = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).linearVelocity;
        v.x = x;
        v.y = y;
        v.z = z;
    }

    static void impl_addForce(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &f = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).force;
        f.x += x;
        f.y += y;
        f.z += z;
    }

    static void impl_addImpulse(void *r, EntityID id, float x, float y, float z)
    {
        if (!reg(r)->hasComponent<ECS::PhysicsRuntimeData>(id))
            return;
        auto &f = reg(r)->getComponent<ECS::PhysicsRuntimeData>(id).impulse;
        f.x += x;
        f.y += y;
        f.z += z;
    }

    // Singleton
    static constexpr ScriptECSAPI API
    {
        impl_has, impl_get, impl_add, impl_remove,
        impl_createEntity, impl_destroyEntity,
        impl_getParent, impl_getFirstChild, impl_getNextSibling,
        impl_getMeshPath, impl_getTexturePath,
        impl_getTag, impl_setTag,
        impl_getLinearVelocity, impl_setLinearVelocity, impl_addForce, impl_addImpulse
    };

    const ScriptECSAPI *getScriptECSAPI()
    {
        return &API;
    }
} // namespace gcep