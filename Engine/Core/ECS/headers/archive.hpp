#pragma once

// Internals
#include <Maths/vector2.hpp>
#include <Maths/vector3.hpp>
#include <Maths/mat3.hpp>
#include <Maths/mat4.hpp>

// STL
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace gcep::SER
{
    class IArchive
    {
    public:
        virtual ~IArchive() = default;

        virtual std::streampos tell() = 0;
        virtual void seek(std::streampos pos) = 0;
        virtual void skip(uint32_t bytes) = 0;

        virtual void writeFloat(float val)   = 0;
        virtual void writeDouble(double val) = 0;

        virtual void writeBool(bool val)       = 0;
        virtual void writeUint8(uint8_t val)   = 0;
        virtual void writeUint16(uint16_t val) = 0;
        virtual void writeUint32(uint32_t val) = 0;
        virtual void writeUint64(uint64_t val) = 0;

        virtual void writeInt8(int8_t val)   = 0;
        virtual void writeInt16(int16_t val) = 0;
        virtual void writeInt32(int32_t val) = 0;
        virtual void writeInt64(int64_t val) = 0;

        virtual void writeSize(uint32_t val) = 0;
        virtual void writeName(const std::string& name) = 0;

        virtual bool   readBool()   = 0;

        virtual float  readFloat()  = 0;
        virtual double readDouble() = 0;

        virtual uint8_t  readUint8()  = 0;
        virtual uint16_t readUint16() = 0;
        virtual uint32_t readUint32() = 0;
        virtual uint64_t readUint64() = 0;

        virtual int8_t  readInt8()  = 0;
        virtual int16_t readInt16() = 0;
        virtual int32_t readInt32() = 0;
        virtual int64_t readInt64() = 0;

        virtual size_t      readSize() = 0;
        virtual std::string readName() = 0;
    };

    class FileArchive : public IArchive
    {
    public:
        explicit FileArchive(const std::string& filename, bool isWriting = true);
        ~FileArchive() override;

        std::streampos tell() override;
        void seek(std::streampos pos) override;
        void skip(uint32_t bytes) override;

        void writeFloat(float val)   override;
        void writeDouble(double val) override;

        void writeBool(bool val)       override;
        void writeUint8(uint8_t val)   override;
        void writeUint16(uint16_t val) override;
        void writeUint32(uint32_t val) override;
        void writeUint64(uint64_t val) override;

        void writeInt8(int8_t val)   override;
        void writeInt16(int16_t val) override;
        void writeInt32(int32_t val) override;
        void writeInt64(int64_t val) override;

        void writeSize(uint32_t val) override;
        void writeName(const std::string& name) override;

        bool  readBool()   override;

        float  readFloat()  override;
        double readDouble() override;

        uint8_t  readUint8()  override;
        uint16_t readUint16() override;
        uint32_t readUint32() override;
        uint64_t readUint64() override;

        int8_t  readInt8()  override;
        int16_t readInt16() override;
        int32_t readInt32() override;
        int64_t readInt64() override;

        size_t      readSize() override;
        std::string readName() override;

    private:
        std::fstream m_file;
        std::string  m_filename;
        bool         m_isWriting;
    };
} // namespace gcep::SER

#include <ECS/detail/archive.inl>
