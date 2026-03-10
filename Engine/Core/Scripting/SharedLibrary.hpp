/*#pragma once

#include <filesystem>
#include <string>

namespace gcep::scripting
{
    class SharedLibrary
    {
    public:
        SharedLibrary() = default;
        ~SharedLibrary();

        SharedLibrary(const SharedLibrary&) = delete;
        SharedLibrary& operator=(const SharedLibrary&) = delete;

        SharedLibrary(SharedLibrary&& other) noexcept;
        SharedLibrary& operator=(SharedLibrary&& other) noexcept;

        bool load(const std::filesystem::path& path, std::string* error);
        void unload();
        [[nodiscard]] void* getSymbol(const char* name, std::string* error) const;
        [[nodiscard]] bool isLoaded() const;

    private:
        void* m_handle = nullptr;
    };
}

*/