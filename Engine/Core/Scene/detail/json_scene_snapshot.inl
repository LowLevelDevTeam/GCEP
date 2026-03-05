#pragma once

namespace gcep::SLS
{
    inline void JsonSnapShot::serializeAll(const std::string& path)
    {
        SER::JsonWriteArchive ar(path);
        ar.beginScene("GCEP_SCENE", 1);

        for (const auto& entry : ECS::ComponentRegistry::instance().entries())
            entry.serializeJson(m_scene.getRegistry(), ar);
    }

    inline void JsonSnapShot::deserializeAll(const std::string& path)
    {
        SER::JsonReadArchive ar(path);

        for (const auto& pool : ar.pools())
        {
            for (const auto& entry : ECS::ComponentRegistry::instance().entries())
            {
                if (entry.name != pool.name) continue;
                for (const auto& ed : pool.entities)
                {
                    m_scene.getRegistry().createEntityWithID(ed.id);
                    entry.deserializeJson(m_scene.getRegistry(), ed.id, ed);
                }
            }
        }
    }
}
