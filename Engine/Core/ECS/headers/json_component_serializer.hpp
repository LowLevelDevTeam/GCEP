#pragma once

// Internals
#include <ECS/headers/json_archive.hpp>
#include <ECS/headers/json_archive_field.hpp>

// Externals
#include <boost/pfr.hpp>

// STL
#include <string>

namespace gcep::SER
{
    template<typename T>
    struct JsonComponentSerializer
    {
        static void serialize(JsonWriteArchive& ar, const T& component)
        {
            constexpr auto names = boost::pfr::names_as_array<T>();
            constexpr std::size_t N = boost::pfr::tuple_size_v<T>;
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (archiveWriteJson(ar, std::string(names[I]),
                    boost::pfr::get<I>(component)), ...);
            }(std::make_index_sequence<N>{});
        }

        static T deserialize(const JsonReadArchive::EntityData& ed)
        {
            T component{};
            constexpr auto names = boost::pfr::names_as_array<T>();
            constexpr std::size_t N = boost::pfr::tuple_size_v<T>;
            [&]<std::size_t... I>(std::index_sequence<I...>) {
                (archiveReadJson(ed, std::string(names[I]),
                    boost::pfr::get<I>(component)), ...);
            }(std::make_index_sequence<N>{});
            return component;
        }
    };
} // namespace gcep::SER
