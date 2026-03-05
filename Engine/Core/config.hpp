#pragma once

#include <string_view>
#include <cstdint>

namespace gcep::config
{
    // --- Engine Info ---
    static constexpr std::string_view engineName = "Gaming Campus Engine Paris";
    static constexpr std::string_view engineVersion = "0.1";

    // --- Build Info ---
#ifdef NDEBUG
    static constexpr bool isDebug = false;
    static constexpr std::string_view buildType = "Release";
#else
    static constexpr bool isDebug = true;
    static constexpr std::string_view buildType = "Debug";
#endif

    // --- Platform & Architecture ---
#if defined(_WIN32)
    static constexpr std::string_view platform = "Windows";
#elif defined(__APPLE__)
    static constexpr std::string_view platform = "Apple";
#elif defined(__linux__)
    static constexpr std::string_view platform = "Linux";
#elif defined(__unix__)
    static constexpr std::string_view platform = "Unix";
#else
    static constexpr std::string_view platform = "Unsupported";
    #error Unsupported platform
#endif

#if defined(_M_X64) || defined(__x86_64__)
    static constexpr std::string_view architecture = "x86_64";
    static constexpr int platformBits = 64;
#elif defined(_M_IX86) || defined(__i386__)
    static constexpr std::string_view architecture = "x86";
    static constexpr int platformBits = 32;
#elif defined(__aarch64__)
    static constexpr std::string_view architecture = "ARM64";
    static constexpr int platformBits = 64;
#elif defined(__arm__)
    static constexpr std::string_view architecture = "ARM";
    static constexpr int platformBits = 32;
#else
    static constexpr std::string_view architecture = "Unknown";
    static constexpr int platformBits = 0;
    #pragma message("Your architecture might not support the application.")
#endif

    // --- Compiler Info ---
#if defined(_MSC_VER)
    static constexpr std::string_view compiler = "MSVC";
    static constexpr int compilerVersion = _MSC_VER;
#elif defined(__clang__)
    static constexpr std::string_view compiler = "Clang";
    static constexpr int compilerVersion = (__clang_major__ * 100) + (__clang_minor__ * 10) + __clang_patchlevel__;
#elif defined(__GNUC__)
    static constexpr std::string_view compiler = "GCC";
    static constexpr int compilerVersion = (__GNUC__ * 100) + (__GNUC_MINOR__ * 10) + __GNUC_PATCHLEVEL__;
#else
    static constexpr std::string_view compiler = "Unknown";
    static constexpr int compilerVersion = 0;
    #pragma message("Your compiler might not work with this application.")
#endif

    // --- C++ version ---
#if defined(_MSVC_LANG)
    #define CPP_STD _MSVC_LANG
#else
    #define CPP_STD __cplusplus
#endif

#if CPP_STD > 202302L
    // The official macro value for C++26 is not yet finalized.
    static constexpr std::string_view cppStandard = "C++26 (Experimental)";
#elif CPP_STD == 202302L
    static constexpr std::string_view cppStandard = "C++23";
#elif CPP_STD == 202002L
    static constexpr std::string_view cppStandard = "C++20";
#else
    static constexpr std::string_view cppStandard = "Pre-C++20";
    #pragma message("Your C++ version might not support C++20 features used inside this engine.")
#endif

} // Namespace gcep::config