#pragma once

// Internals
#include <ECS/headers/archive_field.hpp>
#include <ECS/headers/json_archive.hpp>
#include <Maths/mat3.hpp>
#include <Maths/mat4.hpp>
#include <Maths/quaternion.hpp>
#include <Maths/vector2.hpp>
#include <Maths/vector3.hpp>

namespace gcep::SER
{
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, bool     v) { ar.writeBool  (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, float    v) { ar.writeFloat (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, double   v) { ar.writeDouble(key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, uint8_t  v) { ar.writeUint8 (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, uint16_t v) { ar.writeUint16(key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, uint32_t v) { ar.writeUint32(key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, uint64_t v) { ar.writeUint64(key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, int8_t   v) { ar.writeInt8  (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, int16_t  v) { ar.writeInt16 (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, int32_t  v) { ar.writeInt32 (key, v); }
    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, int64_t  v) { ar.writeInt64 (key, v); }

    template<typename T>
    void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const Vector2<T>& v)
    {
        ar.writeFloat(key + ".x", static_cast<float>(v.x));
        ar.writeFloat(key + ".y", static_cast<float>(v.y));
    }

    template<typename T>
    void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const Vector3<T>& v)
    {
        ar.writeFloat(key + ".x", static_cast<float>(v.x));
        ar.writeFloat(key + ".y", static_cast<float>(v.y));
        ar.writeFloat(key + ".z", static_cast<float>(v.z));
    }


    template<typename T>
    std::enable_if_t<std::is_enum_v<T>>
    archiveWriteJson(JsonWriteArchive& ar, const std::string& key, T& v)
    {
        ar.writeInt32(key, static_cast<int32_t>(v));
    }

    template<typename T>
    std::enable_if_t<!std::is_enum_v<T>>
    archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const T& v) {}


    template <typename T>
    void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const Mat3<T>& m)
    {
        for (int i = 0; i < 3; ++i) {
            ar.writeFloat(key + ".col" + std::to_string(i) + ".x", static_cast<float>(m.cols[i].x));
            ar.writeFloat(key + ".col" + std::to_string(i) + ".y", static_cast<float>(m.cols[i].y));
            ar.writeFloat(key + ".col" + std::to_string(i) + ".z", static_cast<float>(m.cols[i].z));
        }
    }

    template <typename T>
    void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const Mat4<T>& m)
    {
        for (int i = 0; i < 4; ++i) {
            ar.writeFloat(key + ".col" + std::to_string(i) + ".x", static_cast<float>(m.cols[i].x));
            ar.writeFloat(key + ".col" + std::to_string(i) + ".y", static_cast<float>(m.cols[i].y));
            ar.writeFloat(key + ".col" + std::to_string(i) + ".z", static_cast<float>(m.cols[i].z));
            ar.writeFloat(key + ".col" + std::to_string(i) + ".w", static_cast<float>(m.cols[i].w));
        }
    }

    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const Quaternion& q)
    {
        ar.writeFloat(key + ".w", q.w);
        ar.writeFloat(key + ".x", q.x);
        ar.writeFloat(key + ".y", q.y);
        ar.writeFloat(key + ".z", q.z);
    }

    inline void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const std::string& str)
    {
        ar.writeString(key, str);
    }

    template<typename T>
    void archiveWriteJson(JsonWriteArchive& ar, const std::string& key, const T* ptr)
    {
        if (ptr == nullptr)
        {
            ar.writeInt8(key + ".__isNull", true);
            return;
        }
        ar.writeInt8(key + ".__isNull", false);
        archiveWriteJson(ar, key, *ptr); // délègue au cas T
    }

    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, bool&     v) { v = ed.readBool  (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, float&    v) { v = ed.readFloat (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, double&   v) { v = ed.readDouble(key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, uint8_t&  v) { v = ed.readUint8 (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, uint16_t& v) { v = ed.readUint16(key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, uint32_t& v) { v = ed.readUint32(key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, uint64_t& v) { v = ed.readUint64(key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, int8_t&   v) { v = ed.readInt8  (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, int16_t&  v) { v = ed.readInt16 (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, int32_t&  v) { v = ed.readInt32 (key); }
    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, int64_t&  v) { v = ed.readInt64 (key); }

    template<typename T>
    void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, Vector2<T>& v)
    {
        v.x = static_cast<T>(ed.readFloat(key + ".x"));
        v.y = static_cast<T>(ed.readFloat(key + ".y"));
    }

    template<typename T>
    void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, Vector3<T>& v)
    {
        v.x = static_cast<T>(ed.readFloat(key + ".x"));
        v.y = static_cast<T>(ed.readFloat(key + ".y"));
        v.z = static_cast<T>(ed.readFloat(key + ".z"));
    }

    inline void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, std::string& v)
    {
        v = ed.readString(key);
    }

    template<typename T>
    std::enable_if_t<std::is_enum_v<T>>
    archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, T& v)
    {
        v = static_cast<T>(ed.readInt32(key));
    }

    template<typename T>
    std::enable_if_t<!std::is_enum_v<T>>
    archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, T& v) {}

    template<typename T>
    void archiveReadJson(const JsonReadArchive::EntityData& ed, const std::string& key, T*& ptr)
    {
        ptr = nullptr;
    }
} // namespace gcep::SER
