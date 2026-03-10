#pragma once
#include "Engine/RHI/Vulkan/VulkanRHI.hpp"
#include <Log/Log.hpp>


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
        std::cerr << "Calling clear()..." << std::endl;
        clear();
        std::cerr << "clear() done" << std::endl;

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
        std::cerr << "Ensuring pool..." << std::endl;
        if (rhi)
        {
            rhi->setRegistry(&m_registry);
            std::cerr << "Mesh Spawned\n";
            auto& registry = m_registry;
            std::cerr << "Ensuring MeshComponent pool..." << std::endl;
            registry.getPool<ECS::MeshComponent>(); // force la création du pool
            std::cerr << "Pool ready, creating view..." << std::endl;
            auto view = registry.view<ECS::MeshComponent>();
            int count = 0;
            for (auto entity : view)
            {
                count++;
                auto& mc = view.get<ECS::MeshComponent>(entity);
                std::cerr << "Entity " << entity << " filepath: '" << mc.filePath << "'" << std::endl;
                std::cerr << "About to spawn entity " << entity << " filepath: '" << mc.filePath << "'" << std::endl;
                if (!mc.filePath.empty())
                {
                    std::cerr << "Calling spawnAsset..." << std::endl;
                    rhi->spawnAsset(mc.filePath.data(), entity, {0,0,0});
                    std::cerr << "spawnAsset done" << std::endl;
                }
            }
            std::cerr << "Total MeshComponents: " << count << std::endl;
        }
    }

    inline void Scene::save(const std::string& path)
    {
        std::string jsonPath = path.substr(0, path.find_last_of('.')) + ".gcmap";
        JsonSnapShot jsonSnapshot(*this);
        jsonSnapshot.serializeAll(jsonPath);

        std::string binPath = path.substr(0, path.find_last_of('.')) + ".gcmapb";
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
    }

    inline std::vector<ECS::EntityID> Scene::getChildren(ECS::EntityID parent)
    {
        std::vector<ECS::EntityID> children;

        if (!m_registry.hasComponent<ECS::HierarchyComponent>(parent))
            return children;

        auto& parentHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(parent);

        ECS::EntityID current = parentHierarchy.firstChild;
        while (current != ECS::INVALID_VALUE)
        {
            children.push_back(current);
            auto& h = m_registry.getComponent<ECS::HierarchyComponent>(current);
            current = h.nextSibling;
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

        childHierarchy.parent = parent;  // ← THIS LINE WAS MISSING

        if (parentHierarchy.firstChild != ECS::INVALID_VALUE)
        {
            auto& firstChildHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(parentHierarchy.firstChild);
            firstChildHierarchy.prevSibling = child;
            childHierarchy.nextSibling = parentHierarchy.firstChild;
        }

        parentHierarchy.firstChild = child;
    }

    inline void Scene::removeParent(ECS::EntityID child)
    {
        if (!m_registry.hasComponent<ECS::HierarchyComponent>(child)) return;

        auto& childHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(child);
        if (childHierarchy.parent == ECS::INVALID_VALUE) return;

        auto& parentHierarchy = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.parent);

        if (childHierarchy.prevSibling != ECS::INVALID_VALUE)
        {
            auto& prev = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.prevSibling);
            prev.nextSibling = childHierarchy.nextSibling;
        }
        else
        {
            parentHierarchy.firstChild = childHierarchy.nextSibling;
        }

        if (childHierarchy.nextSibling != ECS::INVALID_VALUE)
        {
            auto& next = m_registry.getComponent<ECS::HierarchyComponent>(childHierarchy.nextSibling);
            next.prevSibling = childHierarchy.prevSibling;
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

    }
}
