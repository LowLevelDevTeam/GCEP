#include "SharedLibrary.hpp"

#include <utility>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <Windows.h>
#else
    #include <dlfcn.h>
#endif

namespace gcep::scripting
{
    SharedLibrary::~SharedLibrary()
    {
        unload();
    }

    SharedLibrary::SharedLibrary(SharedLibrary&& other) noexcept
        : m_handle(other.m_handle)
    {
        other.m_handle = nullptr;
    }

    SharedLibrary& SharedLibrary::operator=(SharedLibrary&& other) noexcept
    {
        if (this != &other)
        {
            unload();
            m_handle = other.m_handle;
            other.m_handle = nullptr;
        }
        return *this;
    }

    bool SharedLibrary::load(const std::filesystem::path& path, std::string* error)
    {
        unload();

    #if defined(_WIN32)
        HMODULE handle = LoadLibraryA(path.string().c_str());
        if (!handle)
        {
            if (error)
            {
                *error = "LoadLibraryA failed";
            }
            return false;
        }
        m_handle = handle;
    #else
        void* handle = dlopen(path.string().c_str(), RTLD_NOW);
        if (!handle)
        {
            if (error)
            {
                *error = dlerror() ? dlerror() : "dlopen failed";
            }
            return false;
        }
        m_handle = handle;
    #endif
        return true;
    }

    void SharedLibrary::unload()
    {
        if (!m_handle)
        {
            return;
        }
    #if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(m_handle));
    #else
        dlclose(m_handle);
    #endif
        m_handle = nullptr;
    }

    void* SharedLibrary::getSymbol(const char* name, std::string* error) const
    {
        if (!m_handle)
        {
            if (error)
            {
                *error = "Library not loaded";
            }
            return nullptr;
        }
    #if defined(_WIN32)
        FARPROC symbol = GetProcAddress(static_cast<HMODULE>(m_handle), name);
        if (!symbol && error)
        {
            *error = "GetProcAddress failed";
        }
        return reinterpret_cast<void*>(symbol);
    #else
        dlerror();
        void* symbol = dlsym(m_handle, name);
        if (!symbol && error)
        {
            const char* message = dlerror();
            *error = message ? message : "dlsym failed";
        }
        return symbol;
    #endif
    }

    bool SharedLibrary::isLoaded() const
    {
        return m_handle != nullptr;
    }
}

