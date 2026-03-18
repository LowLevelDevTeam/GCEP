#pragma once

// Internals
#include <Engine/RHI/Vulkan/vulkan_rhi.hpp>
#include <Log/log.hpp>

namespace gcep::SLS
{
    inline Scene::Scene(const std::string& name) : m_name(name)
    {
        m_registry.getPool<ECS::Transform>().reserve(1000);
        m_registry.getPool<ECS::Tag>().reserve(1000);
        m_registry.getPool<ECS::HierarchyComponent>().reserve(1000);
    }

    inline Scene::~Scene()
    {
        clear();
    }

    inline void Scene::onUpdateEditor(float deltaTime)
    {
        onRender();
        m_registry.update();
    }

    inline void Scene::onUpdateRuntime(float deltaTime)
    {
        onRender();
        m_registry.update();
    }

    inline ECS::EntityID Scene::createEntity(const std::string& name)
    {
        auto entity = m_registry.createEntity();
        m_registry.addComponent<ECS::Tag>(entity, name.empty() ? "Entity" : name);
        m_registry.addComponent<ECS::Transform>(entity);
        m_registry.addComponent<ECS::WorldTransform>(entity);
        m_registry.addComponent<ECS::HierarchyComponent>(entity);
        return entity;
    }

    inline void Scene::load(const std::string& path, rhi::vulkan::VulkanRHI* rhi)
    {
        m_path = path;
        clear();
        std::string basePath = path.substr(0, path.find_last_of('.'));

        #ifdef GCEP_EDITOR
            std::string jsonPath = basePath + ".gcmap";
            std::cerr << "Loading JSON: " << jsonPath << std::endl;
            JsonSnapShot snapshot(*this);
            snapshot.deserializeAll(jsonPath);
            std::cerr << "deserializeAll done" << std::endl;
        #else
            std::string binPath = basePath + ".gcmapb";
            SER::FileArchive ar(binPath, false);
            binarySnapShot snapshot(*this);
            snapshot.deserializeAll(ar);
        #endif
        if (rhi)
        {
            rhi->setRegistry(&m_registry);
            auto& registry = m_registry;
            auto view = registry.view<ECS::MeshComponent>();
            for (auto entity : view)
            {
                auto& mc = view.get<ECS::MeshComponent>(entity);
                if (!mc.filePath.empty())
                {
                    rhi->spawnAsset(mc.filePath.data(), entity, {0,0,0});
                }
            }
            auto pointLightView = registry.view<ECS::PointLightComponent>();
            auto spotLightView = registry.view<ECS::SpotLightComponent>();
            for(auto entity : pointLightView)
            {
                rhi->spawnLight(rhi::vulkan::LightType::Point, entity, {0,0,0});
            }
            for(auto entity : spotLightView)
            {
                rhi->spawnLight(rhi::vulkan::LightType::Spot, entity, {0,0,0});
            }
        }
    }

    inline void Scene::save()
    {
        std::string jsonPath = m_path.substr(0, m_path.find_last_of('.')) + ".gcmap";
        JsonSnapShot jsonSnapshot(*this);
        jsonSnapshot.serializeAll(jsonPath);

        std::string binPath = m_path.substr(0, m_path.find_last_of('.')) + ".gcmapb";
        SER::FileArchive ar(binPath, true);
        binarySnapShot binSnapshot(*this);
        binSnapshot.serializeAll(ar);
    }

    inline void Scene::destroyEntity(ECS::EntityID id)
    {
        for (auto child : getChildren(id))
        {
            destroyEntity(child);
        }

        removeParent(id);
        m_registry.destroyEntity(id);
        Log::info(std::to_string(id) + " destroyed");


    }

    inline std::vector<ECS::EntityID> Scene::getChildren(ECS::EntityID parent)
    {
        std::vector<ECS::EntityID> children;

        if (!m_registry.hasComponent<ECS::HierarchyComponent>(parent))
            return children;

        if (!m_registry.hasComponent<ECS::HierarchyComponent>(parent))
            return children;
        auto& parentHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(parent);

        ECS::EntityID current = parentHierarchy.firstChild;
        while (current != ECS::INVALID_VALUE)
        {
            children.push_back(current);
            if (m_registry.hasComponent<ECS::HierarchyComponent>(current)) {
                auto& h = m_registry.getComponent<ECS::HierarchyComponent>(current);
                current = h.nextSibling;
            } else {
                break;
            }
        }

        return children;
    }

    inline ECS::EntityID Scene::getEntity(const std::string& name)
    {
        auto view = m_registry.view<ECS::Tag>();
        for (auto entity : view)
        {
            if (view.get<ECS::Tag>(entity).name == name)
                return entity;
        }
        return ECS::INVALID_VALUE;
    }

    inline void Scene::setParent(ECS::EntityID child, ECS::EntityID parent)
    {
        if (!m_registry.hasComponent<ECS::HierarchyComponent>(child) ||
            !m_registry.hasComponent<ECS::HierarchyComponent>(parent))
            return;
        removeParent(child);

        auto& childHierarchy  = m_registry.getComponent<ECS::HierarchyComponent>(child);
        auto& parentHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(parent);

        childHierarchy.parent = parent;  //  THIS LINE WAS MISSING

        if (parentHierarchy.firstChild != ECS::INVALID_VALUE)
        {
            if (m_registry.hasComponent<ECS::HierarchyComponent>(parentHierarchy.firstChild)) {
                auto& firstChildHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(parentHierarchy.firstChild);
                firstChildHierarchy.prevSibling = child;
                childHierarchy.nextSibling = parentHierarchy.firstChild;
            }
        }

        parentHierarchy.firstChild = child;
    }

    inline void Scene::removeParent(ECS::EntityID child)
    {
        if (!m_registry.hasComponent<ECS::HierarchyComponent>(child)) return;

        auto& childHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(child);
        if (childHierarchy.parent == ECS::INVALID_VALUE) return;

        if (!m_registry.hasComponent<ECS::HierarchyComponent>(childHierarchy.parent)) return;
        auto& parentHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.parent);

        if (childHierarchy.prevSibling != ECS::INVALID_VALUE)
        {
            if (m_registry.hasComponent<ECS::HierarchyComponent>(childHierarchy.prevSibling)) {
                auto& prev = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.prevSibling);
                prev.nextSibling = childHierarchy.nextSibling;
            }
        }
        else
        {
            parentHierarchy.firstChild = childHierarchy.nextSibling;
        }

        if (childHierarchy.nextSibling != ECS::INVALID_VALUE)
        {
            if (m_registry.hasComponent<ECS::HierarchyComponent>(childHierarchy.nextSibling)) {
                auto& next = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.nextSibling);
                next.prevSibling = childHierarchy.prevSibling;
            }
        }

        childHierarchy.parent      = ECS::INVALID_VALUE;
        childHierarchy.prevSibling = ECS::INVALID_VALUE;
        childHierarchy.nextSibling = ECS::INVALID_VALUE;
    }

    inline void Scene::clear()
    {
        for (auto& pool : m_registry.getPools())
        {
            if (!pool) continue;
            auto entities = pool->getEntities();
            for (auto entity : entities)
            {
                m_registry.destroyEntity(entity);
            }
        }
        m_registry.update();

    #ifdef GCEP_EDITOR
        gcep::editor::EditorContext::get().selection.unselect();
        #include <Scripts/ScriptRegistryUtils.hpp>
        gcep::scripting::clearScriptRegistry();
    #endif
    }
} // namespace gcep::SLS
