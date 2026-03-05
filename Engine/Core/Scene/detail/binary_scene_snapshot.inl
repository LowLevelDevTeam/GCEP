#include <optional>

namespace gcep::SLS
{

inline void binarySnapShot::serializeAll(SER::FileArchive& archive)
{
    const auto& registry = m_scene.getRegistry();
    const auto& pools    = registry.getPools();

    archive.writeName("GCEP_SCENE");
    archive.writeUint64(1); // version

    uint32_t poolCount = 0;
    for (auto& pool : pools)
        if (pool && !pool->getEntities().empty()) poolCount++;

    archive.writeSize(poolCount);

    for (auto& pool : pools)
    {
        if (!pool) continue;
        const auto& entities = pool->getEntities();
        if (entities.empty()) continue;

        archive.writeName(pool->getName());
        archive.writeSize(static_cast<uint32_t>(entities.size()));

        for (const auto& entity : entities)
        {
            archive.writeUint32(entity & 0xFFFFFF);

            std::streampos sizePos   = archive.tell();
            archive.writeUint32(0); // placeholder byte-size

            std::streampos dataStart = archive.tell();
            pool->serializeEntity(entity, archive);
            std::streampos dataEnd   = archive.tell();

            uint32_t byteCount = static_cast<uint32_t>(dataEnd - dataStart);
            archive.seek(sizePos);
            archive.writeUint32(byteCount);
            archive.seek(dataEnd);
        }
    }
}

inline void binarySnapShot::deserializeAll(SER::FileArchive& archive)
{
    auto& registry = m_scene.getRegistry(); // plus de const_cast

    if (archive.readName() != "GCEP_SCENE")
        throw std::runtime_error("Format invalide");

    archive.readUint64();
    uint32_t poolCount = archive.readSize();

    for (uint32_t i = 0; i < poolCount; ++i)
    {
        std::string poolName    = archive.readName();
        uint32_t    entityCount = archive.readSize();


        std::optional<ECS::ComponentRegistry::Entry> entry;
        for (const auto& e : ECS::ComponentRegistry::instance().entries())
            if (e.name == poolName) { entry = e; break; }

        for (uint32_t j = 0; j < entityCount; ++j)
        {
            uint32_t rawID    = archive.readUint32();
            uint32_t byteSize = archive.readUint32();

            if (entry)
            {
                registry.createEntityWithID(rawID);
                entry->deserialize(registry, rawID, archive);
            }
            else
            {
                archive.skip(byteSize); // composant inconnu → on saute
            }
        }
    }
}

} // namespace gcep::SLS
