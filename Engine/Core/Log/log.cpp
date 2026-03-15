#include "Log.hpp"

// STL
#include <chrono>
#include <csignal>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>

// OS-Specific
#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__linux__)
    #include <sys/sysinfo.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <mach/host_info.h>
    #include <mach/mach_host.h>
    #include <mach/mach_init.h>
    #include <sys/sysctl.h>
    #include <unistd.h>
#endif

namespace gcep
{
    // ─────────────────────────────────────────────────────────────────────────────
    //  Static member definitions
    // ─────────────────────────────────────────────────────────────────────────────
    std::mutex                                  Log::s_mutex;
    std::unordered_map<std::string, LogChannel> Log::s_channels;
    std::string                                 Log::s_appName;
    std::string                                 Log::s_crashLogPath;
    std::atomic<vk::raii::PhysicalDevice*>      Log::s_physicalDevice { nullptr };
    std::atomic<bool>                           Log::s_initialized    { false };

    // ─────────────────────────────────────────────────────────────────────────────
    //  Internal helpers
    // ─────────────────────────────────────────────────────────────────────────────
    std::string Log::buildTimeHeader()
    {
        using namespace std::chrono;

        const auto now     = system_clock::now();
        const auto seconds = system_clock::to_time_t(now);
        const auto ms      = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &seconds);
    #else
        localtime_r(&seconds, &tm);
    #endif

        std::ostringstream oss;
        oss << '['
            << std::put_time(&tm, "%Y.%m.%d-%H.%M.%S")
            << ':'
            << std::setw(3) << std::setfill('0') << ms.count()
            << "] ";
        return oss.str();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Internal write (called with s_mutex held)
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::writeEntry(LogChannel&                 channel,
                         LogLevel                    level,
                         std::string_view            message,
                         const std::source_location& location)
    {
        // Check level mask
        if (!hasFlag(channel.levelMask, level))
            return;

        const auto timeHeader = buildTimeHeader();
        const auto levelStr   = levelToString(level);
        const auto filename   = std::filesystem::path(location.file_name()).filename().string();

        channel.file << timeHeader
                     << '[' << levelStr << "] "
                     << '[' << channel.name << "] "
                     << filename << ": "
                     << message << '\n';

        if (level == LogLevel::eError)
        {
            channel.file << "    File     : " << location.file_name()
                         << '(' << location.line() << ':' << location.column() << ")\n"
                         << "    Function : " << location.function_name() << '\n';
        }

        // ── Console mirror ───────────────────────────────────────────────────────
        if (channel.mirrorToConsole)
        {
            auto& out = (level == LogLevel::eError || level == LogLevel::eWarn)
                        ? std::cerr : std::cout;

            out << timeHeader
                << '[' << levelStr << "] "
                << '[' << channel.name << "] "
                << filename << ": "
                << message;

            out << '\n';

            if (level == LogLevel::eError)
            {
                out << "    File     : " << location.file_name()
                    << '(' << location.line() << ':' << location.column() << ")\n"
                    << "    Function : " << location.function_name() << '\n';
            }
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Public API — Initialisation
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::initialize(std::string appName, std::string crashLogPath)
    {
        if (s_initialized.exchange(true))
            return; // already initialised — safe no-op

        {
            std::lock_guard lock(s_mutex);
            s_appName      = std::move(appName);
            s_crashLogPath = std::move(crashLogPath);

            // Always ensure a "default" channel exists (writes to a default.log
            // alongside the crash log).
            const auto dir         = std::filesystem::path(s_crashLogPath).parent_path();
            const auto defaultPath = (dir / "default.log").string();

            LogChannel ch;
            ch.name           = "default";
            ch.mirrorToConsole = true;
            ch.levelMask      = LogLevel::eAll;
            ch.file.open(defaultPath, std::ios::out | std::ios::trunc);
            s_channels.emplace("default", std::move(ch));
            writeEntry(s_channels["default"], LogLevel::eCommon, std::string_view("Log file open"), std::source_location::current());
        }

        // Register OS signal handlers
        signal(SIGSEGV, signalHandler);
        signal(SIGILL,  signalHandler);
        signal(SIGFPE,  signalHandler);
        signal(SIGABRT, signalHandler);

    }

    void Log::setVulkanDevice(vk::raii::PhysicalDevice* physicalDevice)
    {
        s_physicalDevice.store(physicalDevice, std::memory_order_relaxed);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Channel management
    // ─────────────────────────────────────────────────────────────────────────────
    bool Log::openChannel(std::string_view  channelName,
                          const std::string& filePath,
                          LogLevel           levelMask,
                          bool               mirrorToConsole)
    {
        std::lock_guard lock(s_mutex);

        LogChannel ch;
        ch.name            = std::string(channelName);
        ch.levelMask       = levelMask;
        ch.mirrorToConsole = mirrorToConsole;
        ch.file.open(filePath, std::ios::out | std::ios::trunc);

        if (!ch.file.is_open())
            return false;

        s_channels[ch.name] = std::move(ch);
        return true;
    }

    void Log::setChannelLevel(std::string_view channelName, LogLevel levelMask)
    {
        std::lock_guard lock(s_mutex);
        if (auto it = s_channels.find(std::string(channelName)); it != s_channels.end())
            it->second.levelMask = levelMask;
    }

    void Log::closeChannel(std::string_view channelName)
    {
        std::lock_guard lock(s_mutex);
        if (auto it = s_channels.find(std::string(channelName)); it != s_channels.end())
        {
            it->second.file.flush();
            s_channels.erase(it);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Core logging
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::log(std::string_view              channelName,
                  LogLevel                      level,
                  std::string_view              message,
                  const std::source_location&   location)
    {
        std::lock_guard lock(s_mutex);
        auto it = s_channels.find(std::string(channelName));
        if (it == s_channels.end())
            return; // silently ignore unknown channels

        writeEntry(it->second, level, message, location);
    }

    void Log::info   (std::string_view msg, const std::source_location& loc) { log("default", LogLevel::eCommon,  msg, loc); }
    void Log::warn   (std::string_view msg, const std::source_location& loc) { log("default", LogLevel::eWarn,    msg, loc); }
    void Log::error  (std::string_view msg, const std::source_location& loc) { log("default", LogLevel::eError,   msg, loc); }
    void Log::verbose(std::string_view msg, const std::source_location& loc) { log("default", LogLevel::eVerbose, msg, loc); }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Flush
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::flushAll()
    {
        std::lock_guard lock(s_mutex);
        for (auto& [name, ch] : s_channels)
            ch.file.flush();
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  System information
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::logSystemInfo(std::ofstream& out)
    {
        out << "Application : " << s_appName    << '\n'
            << "Timestamp   : " << buildTimeHeader() << '\n'
            << "==== System info ====\n";

    #if defined(_WIN32)
        out << "OS          : Windows\n";
    #elif defined(__linux__)
        out << "OS          : Linux\n";
    #elif defined(__APPLE__)
        out << "OS          : macOS\n";
    #else
        out << "OS          : Unknown\n";
    #endif

        out << "CPU Cores   : " << std::thread::hardware_concurrency() << '\n'
            << "==== RAM Memory ====\n";

    #if defined(_WIN32)
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        if (GlobalMemoryStatusEx(&mem))
        {
            out << "Total : " << mem.ullTotalPhys / (1024 * 1024) << " MB\n"
                << "Free  : " << mem.ullAvailPhys / (1024 * 1024) << " MB\n";
        }
    #elif defined(__linux__)
        struct sysinfo info{};
        if (sysinfo(&info) == 0)
        {
            const auto total = static_cast<unsigned long long>(info.totalram) * info.mem_unit;
            const auto free  = static_cast<unsigned long long>(info.freeram)  * info.mem_unit;
            out << "Total : " << total / (1024 * 1024) << " MB\n"
                << "Free  : " << free  / (1024 * 1024) << " MB\n";
        }
    #elif defined(__APPLE__)
        uint64_t total = 0;
        size_t   len   = sizeof(total);
        sysctlbyname("hw.memsize", &total, &len, nullptr, 0);

        mach_port_t           host      = mach_host_self();
        vm_size_t             page_size = 0;
        vm_statistics64_data_t vm_stat{};
        mach_msg_type_number_t count    = HOST_VM_INFO64_COUNT;
        host_page_size(host, &page_size);
        host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stat), &count);
        const uint64_t free = static_cast<uint64_t>(vm_stat.free_count) * page_size;
        out << "Total : " << total / (1024 * 1024) << " MB\n"
            << "Free  : " << free  / (1024 * 1024) << " MB\n";
    #endif
        out << "==== End of system info ====\n";
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Vulkan information
    // ─────────────────────────────────────────────────────────────────────────────
    void Log::logVulkanInfo(std::ofstream& out, vk::raii::PhysicalDevice* dev)
    {
        out << '\n' << "==== Vulkan info ====\n";

        if (!dev)
        {
            out << "No Vulkan physical device available\n"
                << "==== End of Vulkan info ====\n\n";
            return;
        }

        const auto props = dev->getProperties();
        out << "GPU            : " << props.deviceName                         << '\n'
            << "Driver Version : " << props.driverVersion                      << '\n'
            << "Vulkan API     : "
            << VK_VERSION_MAJOR(props.apiVersion) << '.'
            << VK_VERSION_MINOR(props.apiVersion) << '.'
            << VK_VERSION_PATCH(props.apiVersion) << '\n'
            << "Device Type    : ";

        switch (props.deviceType)
        {
            case vk::PhysicalDeviceType::eDiscreteGpu:   out << "Discrete GPU\n";   break;
            case vk::PhysicalDeviceType::eIntegratedGpu: out << "Integrated GPU\n"; break;
            case vk::PhysicalDeviceType::eVirtualGpu:    out << "Virtual GPU\n";    break;
            case vk::PhysicalDeviceType::eCpu:           out << "CPU\n";            break;
            default:                                     out << "Other\n";          break;
        }

        // Memory heaps
        const auto memProps = dev->getMemoryProperties();
        out << "VRAM Heaps     :\n";
        for (uint32_t i = 0; i < memProps.memoryHeapCount; ++i)
        {
            const auto& heap = memProps.memoryHeaps[i];
            if (heap.flags & vk::MemoryHeapFlagBits::eDeviceLocal)
            {
                out << "  Heap[" << i << "] Device-local : " << heap.size / (1024 * 1024) << " MB\n";
            }
        }

        out << "==== End of Vulkan info ====\n\n";
    }

    void Log::handleException(const std::exception& e)
    {
        try
        {
            std::ofstream out(s_crashLogPath);
            out << "==== Crash Report ====\n";
            logSystemInfo(out);
            logVulkanInfo(out, s_physicalDevice.load());

            out << "Exception : " << typeid(e).name() << '\n'
                << "What      : " << e.what()         << '\n';

            out << "==== End of Crash Report ====\n";
        }
        catch (...)
        {
            std::cerr << "Failed to write crash report (exception handler)\n";
        }
    }

    //  Crash reporting — OS signal
    void Log::signalHandler(int signalNum)
    {
        // Signal-safe subset only — no heap allocation ideally, but we still try.
        try
        {
            std::ofstream out(s_crashLogPath);
            out << "==== Crash Report (Signal) ====\n";
            logSystemInfo(out);

            out << "Signal : " << signalNum << " (";
            switch (signalNum)
            {
                case SIGSEGV:
                    out << "SIGSEGV - Segmentation fault";
                    break;
                case SIGILL:
                    out << "SIGILL  - Illegal instruction";
                    break;
                case SIGFPE:
                    out << "SIGFPE  - Floating-point exception";
                    break;
                case SIGABRT:
                    out << "SIGABRT - Abort";
                    break;
                default:
                    out << "Unknown";
                    break;
            }
            out << ")\n==== End of Crash Report ====\n";
        }
        catch (...)
        {
            std::cerr << "Failed to write crash report (signal handler)\n";
        }

        signal(signalNum, SIG_DFL);
        raise(signalNum);
    }
} // namespace gcep
