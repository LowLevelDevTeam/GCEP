#pragma once
#include <string>
#include <fstream>
#include <cstdint>
#include <vector>
#include <stdexcept>

#include <Externals/rapidjson/document.h>
#include <Externals/rapidjson/prettywriter.h>
#include <Externals/rapidjson/stringbuffer.h>
#include <Externals/rapidjson/istreamwrapper.h>

namespace gcep::SER
{

    class JsonWriteArchive
    {
    public:
        explicit JsonWriteArchive(const std::string& filename);
        ~JsonWriteArchive();

        void save();

        // Scene
        void beginScene(const std::string& magic, uint64_t version);

        // Pool
        void beginPool(const std::string& name);
        void endPool();

        // Entity
        void beginEntity(uint32_t id);
        void endEntity();

        // Scalaires
        void writeFloat (const std::string& key, float    val);
        void writeDouble(const std::string& key, double   val);
        void writeUint8 (const std::string& key, uint8_t  val);
        void writeUint16(const std::string& key, uint16_t val);
        void writeUint32(const std::string& key, uint32_t val);
        void writeUint64(const std::string& key, uint64_t val);
        void writeInt8  (const std::string& key, int8_t   val);
        void writeInt16 (const std::string& key, int16_t  val);
        void writeInt32 (const std::string& key, int32_t  val);
        void writeInt64 (const std::string& key, int64_t  val);
        void writeString (const std::string& key, const std::string& val);

    private:
        std::string                       m_filename;
        rapidjson::Document               m_doc;
        rapidjson::Document::AllocatorType& m_alloc;

        // Pointeurs vers les objets courants (non-owning)
        rapidjson::Value* m_poolsArray   = nullptr;
        rapidjson::Value* m_currentPool  = nullptr;
        rapidjson::Value* m_entitiesArray = nullptr;
        rapidjson::Value* m_currentEntity = nullptr;

        bool m_saved = false;
    };





    class JsonReadArchive
    {
    public:
        explicit JsonReadArchive(const std::string& filename);

        const std::string& magic()   const { return m_magic;   }
        uint64_t           version() const { return m_version; }

        // ── Entité parsée ─────────────────────────────────────────────────────
        struct EntityData
        {
            uint32_t id = 0;
            const rapidjson::Value* obj = nullptr; // objet JSON de l'entité

            float    readFloat (const std::string& key) const;
            double   readDouble(const std::string& key) const;
            uint8_t  readUint8 (const std::string& key) const;
            uint16_t readUint16(const std::string& key) const;
            uint32_t readUint32(const std::string& key) const;
            uint64_t readUint64(const std::string& key) const;
            int8_t   readInt8  (const std::string& key) const;
            int16_t  readInt16 (const std::string& key) const;
            int32_t  readInt32 (const std::string& key) const;
            int64_t  readInt64 (const std::string& key) const;
            std::string readString(const std::string& key) const;
        };

        // ── Pool parsé ────────────────────────────────────────────────────────
        struct PoolData
        {
            std::string             name;
            std::vector<EntityData> entities;
        };

        const std::vector<PoolData>& pools() const { return m_pools; }

    private:
        rapidjson::Document  m_doc;
        std::string          m_magic;
        uint64_t             m_version = 0;
        std::vector<PoolData> m_pools;

        void parse();
    };
}


#include <Engine/Core/ECS/detail/json_archive.inl>