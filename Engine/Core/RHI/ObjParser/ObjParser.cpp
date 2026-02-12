#include "ObjParser.hpp"

#include <chrono>
#include <fstream>
#include <iostream>

namespace gcep::objParser
{

ObjFile::ObjFile(std::filesystem::path& filepath)
{
    assert(filepath.string().length() > 0 && "Invalid file path!");
    std::fstream objFile(filepath.string());

    if (!objFile.is_open())
    {
        std::cerr << "Couldn't load " << filepath.string() << '\n';
        return;
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

    pos.reserve(vCount);
    uv.reserve(vtCount);
    normal.reserve(vnCount);
    faces.reserve(fCount);

    while (std::getline(objFile, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string value;
        std::getline(ss, value, ' ');
        std::string elem = trim(value);

        if (elem == "o")
        {
            name = line.substr(2, value.length() - 2);
        }

        else if (elem == "v")
        {
            float posX, posY, posZ;
            ss >> posX >> posY >> posZ;
            pos.emplace_back(glm::vec3(posX, posY, posZ));
        }

        else if (elem == "vt")
        {
            float uvX, uvY;
            ss >> uvX >> uvY;
            uv.emplace_back(glm::vec2(uvX, uvY));
        }

        else if (elem == "vn")
        {
            float normalX, normalY, normalZ;
            ss >> normalX >> normalY >> normalZ;
            normal.emplace_back(glm::vec3(normalX, normalY, normalZ));
        }

        else if (elem == "f")
        {
            // fN = v/vt/vn -> index
            int fPos[3]; int fUV[3]; int fNormal[3];
            std::string vertex;
            for (int i = 0; i < 3; ++i) {
                ss >> vertex;
                std::replace(vertex.begin(), vertex.end(), '/', ' ');
                std::stringstream vss(vertex);
                vss >> fPos[i] >> fUV[i] >> fNormal[i];
                // .obj starts indexing at 1.
                fPos[i]--; fUV[i]--; fNormal[i]--;
            }
            // Reads : v/vt/vn (X) v/vt/vn (Y) v/vt/vn (Z)
            faces.emplace_back(
                std::array{
                    glm::vec3(fPos[0], fUV[0], fNormal[0]),
                    glm::vec3(fPos[1], fUV[1], fNormal[1]),
                    glm::vec3(fPos[2], fUV[2], fNormal[2])
                }
            );
        }
    }

    auto end_timer = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration<float, std::milli>(end_timer - start_timer);

    std::cout << "Loaded obj file in " << total_duration.count() << "ms." << '\n';

    objFile.close();
}

size_t ObjFile::getLinesCount(std::fstream& file) {
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

inline std::string ObjFile::trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

} // Namespace gcep