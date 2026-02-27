#include "Log.hpp"

// STL
#include <csignal>
#include <ostream>
#include <string_view>
#include <thread>


static std::string app_name;
static std::string crash_log_path;
static bool initialized;

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

std::string formatEpoch(std::time_t epoch)
{
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &epoch);
#else
    localtime_r(&epoch, &tm);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm, "%d/%m/%Y %H:%M:%S");
    return oss.str();
}

namespace gcep
{
    void Log::log(std::ofstream& log, const std::string_view message, const std::source_location location)
    {
        log << std::endl;
        log << "Message: " << message << std::endl;
        log << "    - File: " << location.file_name()
            << '(' << location.line() << ':' << location.column() << ")\n"
            << "    - Function: " << location.function_name()
            << std::endl;
        log << std::endl;
    }

    void Log::logSystemInfo(std::ofstream& log) {
        const auto now = std::time(nullptr);
        log << "Application: " << app_name << std::endl;
        log << "Timestamp: " << formatEpoch(now) << std::endl;

        // Log OS information
        log << "==== System info ====" << std::endl;
        #if defined(_WIN32)
            log << "OS: Windows" << std::endl;
        #elif defined(__linux__)
            log << "OS: Linux" << std::endl;
        #elif defined(__APPLE__)
            log << "OS: macOS" << std::endl;
        #else
            log << "OS: Unknown" << std::endl;
        #endif

        // Log CPU information
        log << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
        log << "==== RAM Memory information ====" << std::endl;
        // Log memory information
        #if defined(_WIN32)
            MEMORYSTATUSEX mem_info;
            mem_info.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&mem_info);
            log << "Total Physical Memory: " << mem_info.ullTotalPhys / (1024 * 1024) << " MB" << std::endl;
            log << "Available Memory: " << mem_info.ullAvailPhys / (1024 * 1024) << " MB" << std::endl;
        #elif defined(__linux__)
            struct sysinfo info;
            if (sysinfo(&info) == 0)
            {
                const unsigned long long total = (unsigned long long)info.totalram * info.mem_unit;
                const unsigned long long free = (unsigned long long)info.freeram * info.mem_unit;
                log << "Total Physical Memory: " << total / (1024 * 1024) << " MB" << std::endl;
                log << "Available Memory: " << free / (1024 * 1024) << " MB" << std::endl;
            }
        #elif defined(__APPLE__)
            uint64_t total = 0;
            size_t len = sizeof(total);
            sysctlbyname("hw.memsize", &total, &len, nullptr, 0);
            mach_port_t host = mach_host_self();
            vm_size_t page_size = 0;
            host_page_size(host, &page_size);
            vm_statistics64_data_t vm_stat{};
            mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
            host_statistics64(host, HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm_stat), &count);
            uint64_t free = (uint64_t)vm_stat.free_count * page_size;
            log << "Total Physical Memory: " << total / (1024 * 1024) << " MB" << std::endl;
            log << "Available Memory: " << free / (1024 * 1024) << " MB" << std::endl;
        #endif
        log << "==== End of system info ====" << std::endl;
    }

    // Log Vulkan-specific information
    void Log::logVulkanInfo(std::ofstream& log, vk::raii::PhysicalDevice* physical_device)
    {
        if (physical_device)
        {
            auto properties = physical_device->getProperties();
            log << "==== Vulkan info ====" << std::endl;
            log << "GPU: " << properties.deviceName << std::endl;
            log << "Driver Version: " << properties.driverVersion << std::endl;
            log << "Vulkan API Version: "
                << VK_VERSION_MAJOR(properties.apiVersion) << "."
                << VK_VERSION_MINOR(properties.apiVersion) << "."
                << VK_VERSION_PATCH(properties.apiVersion) << std::endl;
            log << "==== End of vulkan info ====" << std::endl;
        }
        else
        {
            log << "No Vulkan physical device information available" << std::endl;
        }
    }

    // Handler for unhandled exceptions
    void Log::handleException(const std::exception& e, vk::raii::PhysicalDevice* physical_device)
    {
        try
        {
            std::ofstream log(crash_log_path, std::ios::app);
            log << "==== Crash Report ====" << std::endl;
            logSystemInfo(log);
            logVulkanInfo(log, physical_device);

            log << "Exception: " << e.what() << std::endl;
            log << "==== End of Crash Report ====" << std::endl << std::endl;

            log.close();
        }
        catch (...)
        {
            std::cerr << "Failed to write crash log" << std::endl;
        }
    }

    // Signal handler for segfaults, etc.
    void Log::signalHandler(int signalNum)
    {
        try
        {
            std::ofstream log(crash_log_path, std::ios::app);
            log << "==== Crash Report ====" << std::endl;
            logSystemInfo(log);

            log << "Signal: " << signalNum << " (";
            switch (signalNum)
            {
                case SIGSEGV: log << "SIGSEGV - Segmentation fault"; break;
                case SIGILL: log << "SIGILL - Illegal instruction"; break;
                case SIGFPE: log << "SIGFPE - Floating point exception"; break;
                case SIGABRT: log << "SIGABRT - Abort"; break;
                default: log << "Unknown signal"; break;
            }
            log << ")" << std::endl;

            log << "==== End of Crash Report ====" << std::endl << std::endl;

            log.close();
        }
        catch (...)
        {
            // Last resort if we can't even write to the log
            std::cerr << "Failed to write crash log" << std::endl;
        }

        // Re-raise the signal for the default handler
        signal(signalNum, SIG_DFL);
        raise(signalNum);
    }

    // Initialize the crash handler
    void Log::initialize(const std::string& application_name, const std::string& log_path)
    {
        if (initialized) return;

        app_name = application_name;
        crash_log_path = log_path;

        // Set up signal handlers
        signal(SIGSEGV, signalHandler);
        signal(SIGILL, signalHandler);
        signal(SIGFPE, signalHandler);
        signal(SIGABRT, signalHandler);

        initialized = true;
    }

} // Namespace gcep