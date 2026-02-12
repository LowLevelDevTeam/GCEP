#include "ObjParser.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>

namespace gcep::objParser
{

ObjLoader::ObjLoader() = default;

std::pair<ObjLoader::attrib_t, std::vector<ObjLoader::index_t>> ObjLoader::loadObj(std::filesystem::path& filepath)
{
    assert(!filepath.string().empty() && "Invalid file path!");
    std::fstream objFile(filepath.string());

    if (!objFile.is_open())
    {
        throw std::runtime_error("Couldn't load 3D model!");
    }
    auto start_timer = std::chrono::high_resolution_clock::now();

    std::string line;

    unsigned int vCount = 0, vtCount = 0, vnCount = 0, fCount = 0;
    while (std::getline(objFile, line))
    {
        if (line.empty()) continue;

        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;

        std::string_view trimmed = std::string_view(line).substr(first);

        if (trimmed.starts_with("v "))       ++vCount;
        else if (trimmed.starts_with("vt ")) ++vtCount;
        else if (trimmed.starts_with("vn ")) ++vnCount;
        else if (trimmed.starts_with("f "))  ++fCount;
    }
    objFile.clear();
    objFile.seekg(0);

    attribs.vertices.clear();
    attribs.texcoords.clear();
    attribs.normals.clear();
    indices.clear();
    attribs.vertices.reserve(vCount * 3);
    attribs.texcoords.reserve(vtCount * 2);
    attribs.normals.reserve(vnCount * 3);
    indices.reserve(fCount * 3);

    while (std::getline(objFile, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string value;
        std::getline(ss, value, ' ');
        std::string elem = trim(value);

        if (elem == "o")
        {
            ss >> name;
        }

        else if (elem == "v")
        {
            float posX, posY, posZ;
            ss >> posX >> posY >> posZ;
            attribs.vertices.emplace_back(posX);
            attribs.vertices.emplace_back(posY);
            attribs.vertices.emplace_back(posZ);
        }

        else if (elem == "vt")
        {
            float coordX, coordY;
            ss >> coordX >> coordY;
            attribs.texcoords.emplace_back(coordX);
            attribs.texcoords.emplace_back(coordY);
        }

        else if (elem == "vn")
        {
            float normalX, normalY, normalZ;
            ss >> normalX >> normalY >> normalZ;
            attribs.normals.emplace_back(normalX);
            attribs.normals.emplace_back(normalY);
            attribs.normals.emplace_back(normalZ);
        }

        else if (elem == "f")
        {
            uint32_t v[3] =  {0, 0, 0};
            uint32_t vt[3] = {0, 0, 0};
            uint32_t vn[3] = {0, 0, 0};
            std::string vertex;
            for (int i = 0; i < 3; ++i) {
                ss >> vertex;

                std::ranges::replace(vertex, '/', ' ');
                std::stringstream vss(vertex);

                vss >> v[i];
                if (vtCount) { vss >> vt[i]; }
                if (vnCount) { vss >> vn[i]; }
                v[i]--;
                if (vtCount) vt[i]--;
                if (vnCount) vn[i]--;

                index_t idx;
                idx.vertex_index = v[i];
                if (vtCount) idx.texcoord_index = vt[i];
                if (vnCount) idx.normal_index = vn[i];
                indices.emplace_back(idx);
            }
        }
    }

    auto end_timer = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration<float, std::milli>(end_timer - start_timer);

    std::cout << "Loaded obj file in " << total_duration.count() << "ms." << '\n';

    objFile.close();

    return std::make_pair(attribs, indices);
}

size_t ObjLoader::getLinesCount(std::fstream& file) {
    size_t total_lines = 0;

    std::stringstream data;
    data << file.rdbuf();
    auto str = data.str();

    for (auto& ch : str)
    {
        if (ch == '\n') total_lines++;
    }

    file.seekg(0);

    return total_lines;
}

inline std::string ObjLoader::trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

} // Namespace gcep::objParser