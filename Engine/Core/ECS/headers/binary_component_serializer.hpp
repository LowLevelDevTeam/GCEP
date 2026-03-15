#pragma once

// Internals
#include <ECS/headers/archive.hpp>
#include <ECS/headers/archive_field.hpp>

// Externals
#include <boost/pfr.hpp>

namespace gcep::SER
{
    template<typename T>
    struct BinarySerializer
    {
        static void serialize(SER::IArchive& ar, const T& component)
        {
            boost::pfr::for_each_field(component, [&](const auto& field) {
                archiveWrite(ar, field);
            });
        }

        static T deserialize(SER::IArchive& ar)
        {
            T component;
            boost::pfr::for_each_field(component, [&](auto& field) {
                archiveRead(ar, field);
            });
            return component;
        }
    };
} // namespace gcep::SER
