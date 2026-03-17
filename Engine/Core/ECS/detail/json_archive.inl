#pragma once

// Externals
#include <Externals/rapidjson/prettywriter.h>
#include <Externals/rapidjson/stringbuffer.h>
#include <Externals/rapidjson/istreamwrapper.h>

// STL
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gcep::SER
{
    // =========================================================================
    // JsonWriteArchive
    // =========================================================================

    inline JsonWriteArchive::JsonWriteArchive(const std::string& filename)
        : m_filename(filename)
        , m_doc()
        , m_alloc(m_doc.GetAllocator())
    {
        m_doc.SetObject();
    }

    inline JsonWriteArchive::~JsonWriteArchive()
    {
        if (!m_saved)
            save();
    }

    inline void JsonWriteArchive::save()
    {
        rapidjson::StringBuffer buf;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
        m_doc.Accept(writer);

        const auto parent = std::filesystem::path(m_filename).parent_path();
        if (!parent.empty())
            std::filesystem::create_directories(parent);

        std::ofstream file(m_filename, std::ios::out | std::ios::trunc);
        if (!file.is_open())
            throw std::runtime_error("JsonWriteArchive: cannot open file: " + m_filename);

        file << buf.GetString();
        m_saved = true;
    }

    inline void JsonWriteArchive::beginScene(const std::string& magic, uint64_t version)
    {
        m_doc.AddMember("magic",   rapidjson::Value(magic.c_str(), m_alloc), m_alloc);
        m_doc.AddMember("version", rapidjson::Value(version),                m_alloc);

        rapidjson::Value pools(rapidjson::kArrayType);
        m_doc.AddMember("pools", pools, m_alloc);
        m_poolsArray = &m_doc["pools"];
    }

    inline void JsonWriteArchive::beginPool(const std::string& name)
    {
        rapidjson::Value pool(rapidjson::kObjectType);
        pool.AddMember("name",     rapidjson::Value(name.c_str(), m_alloc), m_alloc);
        pool.AddMember("entities", rapidjson::Value(rapidjson::kArrayType), m_alloc);

        m_poolsArray->PushBack(pool, m_alloc);
        m_currentPool   = &(*m_poolsArray)[m_poolsArray->Size() - 1] ;
        m_entitiesArray = &(*m_currentPool)["entities"];
    }

    inline void JsonWriteArchive::endPool()
    {
        m_currentPool   = nullptr;
        m_entitiesArray = nullptr;
    }

    inline void JsonWriteArchive::beginEntity(uint32_t id)
    {
        rapidjson::Value entity(rapidjson::kObjectType);
        entity.AddMember("id", rapidjson::Value(id), m_alloc);
        m_entitiesArray->PushBack(entity, m_alloc);
        m_currentEntity = &(*m_entitiesArray)[m_entitiesArray->Size() - 1];
    }

    inline void JsonWriteArchive::endEntity()
    {
        m_currentEntity = nullptr;
    }

    inline void JsonWriteArchive::writeBool(const std::string& key, bool val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeFloat(const std::string& key, float val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeDouble(const std::string& key, double val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeUint8(const std::string& key, uint8_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(static_cast<uint32_t>(val)), m_alloc);
    }

    inline void JsonWriteArchive::writeUint16(const std::string& key, uint16_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(static_cast<uint32_t>(val)), m_alloc);
    }

    inline void JsonWriteArchive::writeUint32(const std::string& key, uint32_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeUint64(const std::string& key, uint64_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeInt8(const std::string& key, int8_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(static_cast<int32_t>(val)), m_alloc);
    }

    inline void JsonWriteArchive::writeInt16(const std::string& key, int16_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(static_cast<int32_t>(val)), m_alloc);
    }

    inline void JsonWriteArchive::writeInt32(const std::string& key, int32_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeInt64(const std::string& key, int64_t val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val), m_alloc);
    }

    inline void JsonWriteArchive::writeString(const std::string& key, const std::string& val)
    {
        m_currentEntity->AddMember(rapidjson::Value(key.c_str(), m_alloc), rapidjson::Value(val.c_str(), m_alloc), m_alloc);
    }
    inline void JsonWriteArchive::writeSceneName(const std::string& name)
    {
        m_doc.AddMember("scene_name", rapidjson::Value(name.c_str(), m_alloc), m_alloc);
    }

    inline JsonReadArchive::JsonReadArchive(const std::string& filename)
    {
        std::ifstream file(filename);
        if (!file.is_open())
            throw std::runtime_error("JsonReadArchive: cannot open file: " + filename);

        rapidjson::IStreamWrapper isw(file);
        m_doc.ParseStream(isw);

        if (m_doc.HasParseError())
            throw std::runtime_error("JsonReadArchive: JSON parse error in: " + filename);

        parse();
    }

    inline void JsonReadArchive::parse()
    {
        if (!m_doc.HasMember("magic") || !m_doc.HasMember("version") || !m_doc.HasMember("pools"))
            throw std::runtime_error("JsonReadArchive: missing required fields");

        m_magic   = m_doc["magic"].GetString();
        m_version = m_doc["version"].GetUint64();

        const auto& pools = m_doc["pools"];
        for (auto& pool : pools.GetArray())
        {
            PoolData pd;
            pd.name = pool["name"].GetString();

            for (auto& entity : pool["entities"].GetArray())
            {
                EntityData ed;
                ed.id  = entity["id"].GetUint();
                ed.obj = &entity;
                pd.entities.push_back(ed);
            }

            m_pools.push_back(std::move(pd));
        }
    }

    // ── EntityData readers ────────────────────────────────────────────────────

    inline bool JsonReadArchive::EntityData::readBool(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetBool();
    }

    inline float JsonReadArchive::EntityData::readFloat(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetFloat();
    }

    inline double JsonReadArchive::EntityData::readDouble(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetDouble();
    }

    inline uint8_t JsonReadArchive::EntityData::readUint8(const std::string& key) const
    {
        return static_cast<uint8_t>((*obj)[key.c_str()].GetUint());
    }

    inline uint16_t JsonReadArchive::EntityData::readUint16(const std::string& key) const
    {
        return static_cast<uint16_t>((*obj)[key.c_str()].GetUint());
    }

    inline uint32_t JsonReadArchive::EntityData::readUint32(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetUint();
    }

    inline uint64_t JsonReadArchive::EntityData::readUint64(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetUint64();
    }

    inline int8_t JsonReadArchive::EntityData::readInt8(const std::string& key) const
    {
        return static_cast<int8_t>((*obj)[key.c_str()].GetInt());
    }

    inline int16_t JsonReadArchive::EntityData::readInt16(const std::string& key) const
    {
        return static_cast<int16_t>((*obj)[key.c_str()].GetInt());
    }

    inline int32_t JsonReadArchive::EntityData::readInt32(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetInt();
    }

    inline int64_t JsonReadArchive::EntityData::readInt64(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetInt64();
    }
    inline std::string JsonReadArchive::EntityData::readString(const std::string& key) const
    {
        return (*obj)[key.c_str()].GetString();
    }
    inline std::string JsonReadArchive::readSceneName() const
    {
        if (m_doc.HasMember("scene_name") && m_doc["scene_name"].IsString())
            return m_doc["scene_name"].GetString();
        return "Unnamed Scene";
    }
} // namespace gcep::SER
