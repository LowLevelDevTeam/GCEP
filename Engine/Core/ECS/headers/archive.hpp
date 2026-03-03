#pragma once
#include <string>
#include <cstddef>
#include "entity_component.hpp"

namespace gcep::ECS {

    /**
     * @class IArchive
     * @brief Interface pour sérialiser/désérialiser
     * 
     * À hériter pour créer FileArchive, JSONArchive, etc
     */
    class IArchive {
    public:
        virtual ~IArchive() = default;

        // ===== ÉCRITURE =====
        virtual void writeFloat(float val) = 0;
        virtual void writeDouble(double val) = 0;
        virtual void writeUint8(uint8_t val) = 0;
        virtual void writeUint16(uint16_t val) = 0;
        virtual void writeUint32(uint32_t val) = 0;
        virtual void writeUint64(uint64_t val) = 0;

        virtual void writeSize(uint32_t val) = 0;
        virtual void writeName(const std::string& name) = 0;
        virtual void writeEntity(EntityID id) = 0;

        // ===== LECTURE =====
        virtual float readFloat() = 0;
        virtual double readDouble() = 0;
        virtual int readUint8(uint8_t val) = 0;
        virtual int readUint16(uint16_t val) = 0;
        virtual int readUint32(uint32_t val) = 0;
        virtual int readUint64(uint64_t val) = 0;
        virtual size_t readSize() = 0;
        virtual std::string readName() = 0;
    };

    /**
     * @class FileArchive
     * @brief Archive binaire (fichier)
     * Implémente IArchive en écrivant/lisant un fichier
     */
    class FileArchive : public IArchive {
    public:
        explicit FileArchive(const std::string& filename, bool isWriting = true);
        ~FileArchive() override;

        // Implémentations des méthodes virtuelles
        void writeFloat(float val) override;
        void writeDouble(double val) override;
        void writeUint8(uint8_t val) override;
        void writeUint16(uint16_t val) override;
        void writeUint32(uint32_t val) override;
        void writeUint64(uint64_t val) override;
        void writeSize(uint32_t val) override;
        void writeName(const std::string& name) override;

        float readFloat() override;
        double readDouble() override;
        int readUint8(uint8_t val) override;
        int readUint16(uint16_t val) override;
        int readUint32(uint32_t val) override;
        int readUint64(uint64_t val) override;
        size_t readSize() override;
        std::string readName() override;

    private:
        std::fstream m_file;
        std::string m_filename;
        bool m_isWriting;
    };
#include <Engine/Core/ECS/detail/archive.inl>
}