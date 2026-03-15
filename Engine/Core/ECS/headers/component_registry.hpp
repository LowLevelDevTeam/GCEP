#pragma once

// Internals
#include <ECS/headers/entity_component.hpp>
#include <ECS/headers/json_archive.hpp>

// STL
#include <functional>
#include <string>
#include <vector>

// Forward declarations
namespace gcep::SER
{
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
            uint32_t    typeID;
            std::string name;
            std::function<void(Registry&)>                                                   ensurePool;
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
} // namespace gcep::ECS

#include <ECS/detail/component_registry.inl>
