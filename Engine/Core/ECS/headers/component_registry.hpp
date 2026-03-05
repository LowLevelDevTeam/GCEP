// ComponentRegistry.hpp
#pragma once
#include <vector>
#include <string>
#include <functional>
#include <ECS/headers/entity_component.hpp>
#include <ECS/headers/component_pool.hpp>
#include <ECS/headers/json_archive.hpp>
#include <ECS/headers/json_component_serializer.hpp>

namespace gcep::ECS
{
    class Registry;

    class ComponentRegistry
    {
    public:
        struct Entry
        {
            uint32_t     typeID;
            std::string  name;
            std::function<void(Registry&)>                                                    ensurePool;
            std::function<void(Registry&, EntityID, SER::IArchive&)>                          deserialize;
            std::function<void(Registry&, SER::JsonWriteArchive&)>                            serializeJson;
            std::function<void(Registry&, EntityID, const SER::JsonReadArchive::EntityData&)> deserializeJson;
        };

        static ComponentRegistry& instance();

        template<typename T>
        bool reg();

        const std::vector<Entry>& entries() const;

    private:
        ComponentRegistry() = default;
        ComponentRegistry(const ComponentRegistry&)            = delete;
        ComponentRegistry& operator=(const ComponentRegistry&) = delete;
        ComponentRegistry(ComponentRegistry&&)                 = delete;
        ComponentRegistry& operator=(ComponentRegistry&&)      = delete;

        std::vector<Entry> m_entries;
    };


}

#include <Engine/Core/ECS/detail/component_registry.inl>