#pragma once
#ifdef __GNUG__
    #include <cxxabi.h>
    #include <cstdlib>
#endif

// IMPORTANT: Ces includes doivent être AVANT le code pour que ComponentIDGenerator soit connu
#include <ECS/headers/entity_component.hpp>
#include <ECS/headers/json_archive.hpp>
#include <ECS/headers/json_component_serializer.hpp>

namespace gcep::ECS
{
    // Même logique de démangling que ComponentPool<T>::getName()
    // pour que les noms dans le registry matchent ceux écrits dans le fichier.
    inline std::string demangle(const char* mangled)
    {
#ifdef __GNUG__
        int status = 0;
        char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
        if (status == 0 && demangled)
        {
            std::string result(demangled);
            free(demangled);
            return result;
        }
#endif
        return mangled;
    }

    inline ComponentRegistry& ComponentRegistry::instance()
    {
        static ComponentRegistry reg;
        return reg;
    }

    inline const std::vector<ComponentRegistry::Entry>& ComponentRegistry::entries() const
    {
        return m_entries;
    }

    template<typename T>
    bool ComponentRegistry::reg()
    {
        uint32_t id = ComponentIDGenerator::get<T>();

        for (const auto& e : m_entries)
            if (e.typeID == id) return true;

        Entry entry;
        entry.typeID      = id;
        entry.name        = demangle(typeid(T).name());
        entry.ensurePool  = [](Registry& r) { (void)r.getPool<T>(); };
        entry.deserialize = [](Registry& r, EntityID e, ::gcep::SER::IArchive& a) { r.getPool<T>().deserializeEntity(e, a); };

        entry.serializeJson = [](Registry& r, ::gcep::SER::JsonWriteArchive& ar)
        {
            auto& pool = r.getPool<T>();
            if (pool.getEntities().empty()) return;
            ar.beginPool(demangle(typeid(T).name()));
            for (auto entity : pool.getEntities())
            {
                ar.beginEntity(entity & 0xFFFFFF);
                ::gcep::SER::JsonComponentSerializer<T>::serialize(ar, pool.get(entity));
                ar.endEntity();
            }
            ar.endPool();
        };

        entry.deserializeJson = [](Registry& r, EntityID id, const ::gcep::SER::JsonReadArchive::EntityData& ed)
        {
            T component = ::gcep::SER::JsonComponentSerializer<T>::deserialize(ed);
            (void)r.getPool<T>().add(id, std::move(component));
        };

        m_entries.push_back(std::move(entry));  // ← tout est rempli, on push à la fin
        return true;
    }

}
