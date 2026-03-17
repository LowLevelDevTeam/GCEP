#pragma once

// Internals
#include <ECS/headers/archive.hpp>
#include <Maths/quaternion.hpp>

namespace gcep::SER
{
    // ── Scalaires ─────────────────────────────────────────────────────────────
    inline void archiveWrite(IArchive& ar, bool     v) { ar.writeBool(v);  }
    inline void archiveWrite(IArchive& ar, uint8_t  v) { ar.writeUint8(v);  }
    inline void archiveWrite(IArchive& ar, uint16_t v) { ar.writeUint16(v); }
    inline void archiveWrite(IArchive& ar, uint32_t v) { ar.writeUint32(v); }
    inline void archiveWrite(IArchive& ar, uint64_t v) { ar.writeUint64(v); }
    inline void archiveWrite(IArchive& ar, float    v) { ar.writeFloat(v);  }
    inline void archiveWrite(IArchive& ar, double   v) { ar.writeDouble(v); }

    // ── Maths ─────────────────────────────────────────────────────────────────
    template<typename T>
    void archiveWrite(IArchive& ar, const Vector2<T>& v) {
        archiveWrite(ar, v.x);
        archiveWrite(ar, v.y);
    }
    template<typename T>
    void archiveWrite(IArchive& ar, const Vector3<T>& v) {
        archiveWrite(ar, v.x);
        archiveWrite(ar, v.y);
        archiveWrite(ar, v.z);
    }
    inline void archiveWrite(IArchive& ar, const Quaternion& q) {
        archiveWrite(ar, q.w);
        archiveWrite(ar, q.x);
        archiveWrite(ar, q.y);
        archiveWrite(ar, q.z);
    }
    template<typename T>
    void archiveWrite(IArchive& ar, const Mat3<T>& mat) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                archiveWrite(ar, mat.m[i][j]);
    }
    template<typename T>
    void archiveWrite(IArchive& ar, const Mat4<T>& mat) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                archiveWrite(ar, mat.m[i][j]);
    }

    inline void archiveWrite(IArchive& ar, const std::string& str)
    {
        ar.writeName(str);
    }

    template<typename T>
    std::enable_if_t<std::is_enum_v<T>>
    archiveWrite(IArchive& ar, T& v)
    {
        ar.writeInt32(static_cast<int32_t>(v));
    }

    template<typename T>
    std::enable_if_t<!std::is_enum_v<T>>
    archiveWrite(IArchive& ar, const T& v) {}

    // ── Lecture ───────────────────────────────────────────────────────────────
    inline void archiveRead(IArchive& ar, bool&     v) { v = ar.readBool();   }
    inline void archiveRead(IArchive& ar, uint8_t&  v) { v = ar.readUint8();  }
    inline void archiveRead(IArchive& ar, uint16_t& v) { v = ar.readUint16(); }
    inline void archiveRead(IArchive& ar, uint32_t& v) { v = ar.readUint32(); }
    inline void archiveRead(IArchive& ar, uint64_t& v) { v = ar.readUint64(); }
    inline void archiveRead(IArchive& ar, float&    v) { v = ar.readFloat();  }
    inline void archiveRead(IArchive& ar, double&   v) { v = ar.readDouble(); }

    template<typename T>
    void archiveRead(IArchive& ar, Vector2<T>& v) {
        archiveRead(ar, v.x);
        archiveRead(ar, v.y);
    }
    template<typename T>
    void archiveRead(IArchive& ar, Vector3<T>& v) {
        archiveRead(ar, v.x);
        archiveRead(ar, v.y);
        archiveRead(ar, v.z);
    }
    inline void archiveRead(IArchive& ar, Quaternion& q) {
        archiveRead(ar, q.w);
        archiveRead(ar, q.x);
        archiveRead(ar, q.y);
        archiveRead(ar, q.z);
    }
    template<typename T>
    void archiveRead(IArchive& ar, Mat3<T>& mat) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                archiveRead(ar, mat.m[i][j]);
    }
    template<typename T>
    void archiveRead(IArchive& ar, Mat4<T>& mat) {
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                archiveRead(ar, mat.m[i][j]);
    }

    inline void archiveRead(IArchive& ar, std::string& str)
    {
        str = ar.readName();
    }

    template<typename T>
    std::enable_if_t<std::is_enum_v<T>>
    archiveRead(IArchive& ar, T& v)
    {
        v = static_cast<T>(ar.readInt32());
    }

    template<typename T>
    std::enable_if_t<!std::is_enum_v<T>>
    archiveRead(IArchive& ar, T& v) {}

} // namespace gcep::SER
