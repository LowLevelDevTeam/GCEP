#pragma once
#include <fstream>
#include <ios>
#include <Engine/Core/ECS/headers/archive.hpp>

namespace gcep::ECS
{
    inline FileArchive::FileArchive(const std::string& filename, bool isWriting) : m_filename(filename), m_isWriting(isWriting)
    {
        std::ios::openmode mode = std::ios::binary;
        if (isWriting)
        {
            mode |= std::ios::out;
        }
        else
        {
            mode |= std::ios::in;
        }
        m_file.open(filename, mode);
        if (!m_file.is_open())
        {
            throw std::runtime_error("Failed to open file for writing");
        }
    }

    inline FileArchive::~FileArchive() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    inline void FileArchive::writeFloat(float val)
    {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(float));
    }

    inline void FileArchive::writeDouble(double val)
    {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(double));
    }

    inline void FileArchive::writeUint8(uint8_t val) {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint8_t));
    }

    inline void FileArchive::writeUint16(uint16_t val) {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint16_t));
    }

    inline void FileArchive::writeUint32(uint32_t val) {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint32_t));
    }

    inline void FileArchive::writeUint64(uint64_t val)
    {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint64_t));
    }

    inline void FileArchive::writeSize(uint32_t val)
    {
        m_file.write(reinterpret_cast<const char*>(&val), sizeof(uint32_t));
    }

    inline void FileArchive::writeName(const std::string& name)
    {
        uint32_t len = static_cast<uint32_t>(name.size());
        m_file.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
        m_file.write(name.data(), name.size());
    }


    inline float FileArchive::readFloat()
    {
        float val;
        m_file.read(reinterpret_cast<char*>(&val), sizeof(float));
        return val;
    }

    inline double FileArchive::readDouble()
    {
        double val;
        m_file.read(reinterpret_cast<char*>(&val), sizeof(double));
        return val;
    }

    inline int FileArchive::readUint8(uint8_t val)
    {
        uint8_t readVal;
        m_file.read(reinterpret_cast<char*>(&readVal), sizeof(uint8_t));
        return readVal;
    }

    inline int FileArchive::readUint16(uint16_t val)
    {
        uint16_t readVal;
        m_file.read(reinterpret_cast<char*>(&readVal), sizeof(uint16_t));
        return readVal;
    }

    inline int FileArchive::readUint32(uint32_t val)
    {
        uint32_t readVal;
        m_file.read(reinterpret_cast<char*>(&readVal), sizeof(uint32_t));
        return readVal;
    }

    inline int FileArchive::readUint64(uint64_t val)
    {
        uint64_t readVal;
        m_file.read(reinterpret_cast<char*>(&readVal), sizeof(uint64_t));
        return readVal;
    }

    inline size_t FileArchive::readSize()
    {
        uint32_t len;
        m_file.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        return len;
    }

    inline std::string FileArchive::readName()
    {
        uint32_t len;
        m_file.read(reinterpret_cast<char*>(&len), sizeof(uint32_t));
        std::string name(len, '\0');
        m_file.read(name.data(), len);
        return name;
    }
}
