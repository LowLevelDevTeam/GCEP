#pragma once
#include <vector>
#include <string>
#include <functional>
#include <ECS/headers/entity_component.hpp>

#include "json_archive.hpp"

// Forward déclarations pour éviter les inclusions circulaires
namespace gcep::SER {
    class IArchive;
    class JsonWriteArchive;
    class JsonReadArchive;
}

namespace gcep::ECS
{
    class Registry;
    using EntityID = std::uint32_t;
    using SER_IArchive = ::gcep::SER::IArchive;
    using SER_JsonWriteArchive = ::gcep::SER::JsonWriteArchive;
    using SER_JsonReadArchive = ::gcep::SER::JsonReadArchive;

    class ComponentRegistry
    {
    public:
        struct Entry
        {
            uint32_t     typeID;
            std::string  name;
            std::function<void(Registry&)>                                                    ensurePool;
            std::function<void(Registry&, EntityID, SER_IArchive&)>                          deserialize;
            std::function<void(Registry&, SER_JsonWriteArchive&)>                            serializeJson;
            std::function<void(Registry&, EntityID, const SER_JsonReadArchive::EntityData&)> deserializeJson;
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

#include <ECS/detail/component_registry.inl>