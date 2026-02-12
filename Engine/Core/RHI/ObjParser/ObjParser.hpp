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

namespace gcep::objParser
{

class ObjLoader {
public:
    typedef struct
    {
        int vertex_index;
        int texcoord_index;
        int normal_index;
    } index_t;
    typedef struct
    {
        std::vector<float> vertices;
        std::vector<float> texcoords;
        std::vector<float> normals;
    } attrib_t;
    static ObjLoader& getInstance()
    {
        static ObjLoader instance;
        return instance;
    }
    std::pair<attrib_t, std::vector<index_t>> loadObj(std::filesystem::path& filepath);

private:
    ObjLoader();
    std::vector<index_t> indices;
    attrib_t attribs;
    std::string name;

    static size_t getLinesCount(std::fstream& file);
    static std::string trim(const std::string& s);
};

} // Namespace gcep::objParser