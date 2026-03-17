#include "prefab_system.hpp"

#include <ECS/headers/component_registry.hpp>
#include <ECS/Components/hierarchy_component.hpp>
#include <ECS/Components/world_transform.hpp>
#include <ECS/Components/tag.hpp>
#include <ECS/headers/json_archive.hpp>
#include <Log/log.hpp>

namespace gcep::editor
{
    // ── Component names to exclude from prefabs ───────────────────────────────
    static bool shouldExclude(const std::string& name)
    {
        return name.find("HierarchyComponent") != std::string::npos ||
               name.find("WorldTransform")     != std::string::npos;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Save
    // ─────────────────────────────────────────────────────────────────────────

    void PrefabSystem::save(ECS::Registry&          registry,
                            ECS::EntityID           id,
                            const std::string&      meshPath,
                            const std::filesystem::path& outPath)
    {
        std::filesystem::create_directories(outPath.parent_path());

        SER::JsonWriteArchive ar(outPath.string());
        ar.beginScene(PREFAB_MAGIC, 1);

        // Store mesh path in a dedicated pseudo-pool
        if (!meshPath.empty())
        {
            ar.beginPool("__mesh__");
            ar.beginEntity(0);
            ar.writeString("path", meshPath);
            ar.endEntity();
            ar.endPool();
        }

        // Serialize each component present on the entity
        for (const auto& entry : ECS::ComponentRegistry::instance().entries())
        {
            if (shouldExclude(entry.name)) continue;
            entry.serializeEntityJson(registry, id, ar);
        }

        ar.save();
        Log::info("Prefab saved: " + outPath.string());
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Instantiate
    // ─────────────────────────────────────────────────────────────────────────

    ECS::EntityID PrefabSystem::instantiate(SLS::Scene&              scene,
                                             rhi::vulkan::VulkanRHI*  rhi,
                                             const std::filesystem::path& path,
                                             glm::vec3                pos)
    {
        SER::JsonReadArchive ar(path.string());
        if (ar.magic() != PREFAB_MAGIC)
        {
            Log::error("PrefabSystem::instantiate — not a prefab file: " + path.string());
            return ECS::INVALID_VALUE;
        }

        // Bare entity (no default components added)
        ECS::Registry& registry = scene.getRegistry();
        const ECS::EntityID id  = registry.createEntity();

        std::string meshPath;

        for (const auto& pool : ar.pools())
        {
            if (pool.name == "__mesh__")
            {
                if (!pool.entities.empty())
                    meshPath = pool.entities[0].readString("path");
                continue;
            }

            for (const auto& entry : ECS::ComponentRegistry::instance().entries())
            {
                if (entry.name != pool.name) continue;
                for (const auto& entityData : pool.entities)
                    entry.deserializeJson(registry, id, entityData);
            }
        }

        // Always ensure hierarchy + world transform components exist
        if (!registry.hasComponent<ECS::HierarchyComponent>(id))
            registry.addComponent<ECS::HierarchyComponent>(id);
        if (!registry.hasComponent<ECS::WorldTransform>(id))
            registry.addComponent<ECS::WorldTransform>(id);

        // Override spawn position
        if (registry.hasComponent<ECS::Transform>(id))
            registry.getComponent<ECS::Transform>(id).position = { pos.x, pos.y, pos.z };

        // Spawn mesh if stored
        if (!meshPath.empty())
            rhi->spawnAsset(const_cast<char*>(meshPath.c_str()), id, pos);

        Log::info("Prefab instantiated from: " + path.string());
        return id;
    }

} // namespace gcep::editor
