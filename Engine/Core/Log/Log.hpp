#pragma once

// Externals
#include <vulkan/vulkan_raii.hpp>

// STL
#include <fstream>
#include <iostream>
#include <source_location>
#include <string_view>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#endif

namespace gcep
{

class Log
{
public:
    // Log at a certain place
    static void log(std::ofstream& log, const std::string_view message, const std::source_location location = std::source_location::current());

    // Log basic system information
    static void logSystemInfo(std::ofstream& log);

    // Log Vulkan-specific information
    static void logVulkanInfo(std::ofstream& log, vk::raii::PhysicalDevice* physical_device = nullptr);

    // Handler for unhandled exceptions
    static void handleException(const std::exception& e, vk::raii::PhysicalDevice* physical_device = nullptr);

    // Signal handler for segfaults, etc.
    static void signalHandler(int signalNum);

    // Initialize the crash handler
    static void initialize(const std::string& application_name, const std::string& log_path);
};

} // Namespace gcep