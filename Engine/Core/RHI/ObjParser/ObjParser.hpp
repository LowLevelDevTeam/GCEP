#pragma once

// Externals
#include <glm/glm.hpp>

// STL
#include <array>
#include <filesystem>
#include <vector>

/*
 * Notes :
 * v  = vertex position (x y z)
 * vt = vertex uv coordinate (u v)
 * vn = vertex normal (x y z)
 * f = face definition (v/vt/vn v/vt/vn v/vt/vn)
 */

namespace gcep
{

namespace objParser
{

class ObjFile {
public:
    explicit ObjFile(std::filesystem::path& filepath);

private:
    using face = std::array<glm::vec3, 3>;
    std::vector<glm::vec3> pos;
    std::vector<glm::vec2> uv;
    std::vector<glm::vec3> normal;
    std::vector<face> faces;

    std::string name;

    static size_t getLinesCount(std::fstream& file);
    static std::string trim(const std::string& s);
};

}

} // Namespace gcep