#include "ObjParser.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>

namespace gcep::objParser
{

std::pair<attrib_t, std::vector<index_t>> ObjLoader::loadObj(std::filesystem::path& filepath)
{
    assert(!filepath.string().empty() && "Invalid file path!");
    assert(filepath.extension() == ".obj" && "File must be .obj!");

    std::fstream objFile(filepath.string());

    if (!objFile.is_open())
    {
        throw std::runtime_error("Couldn't load 3D model!");
    }
    auto start_timer = std::chrono::high_resolution_clock::now();

    std::string line;

    // First pass: count elements
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

    attrib_t attribs;
    std::vector<index_t> indices;

    attribs.vertices.reserve(vCount * 3);
    attribs.texcoords.reserve(vtCount * 2);
    attribs.normals.reserve(vnCount * 3);
    indices.reserve(fCount * 6);

    // Second pass: parse data
    while (std::getline(objFile, line))
    {
        if (line.empty()) continue;

        size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos) continue;

        std::string_view trimmed = std::string_view(line).substr(first);

        if(trimmed.starts_with("o "))
        {
            sscanf(line.c_str() + 2, "%s", name);
        }
        else if (trimmed.starts_with("v "))
        {
            float x, y, z;
            sscanf(line.c_str() + 2, "%f %f %f", &x, &y, &z);
            attribs.vertices.push_back(x);
            attribs.vertices.push_back(y);
            attribs.vertices.push_back(z);
        }
        else if (trimmed.starts_with("vt "))
        {
            float u, v;
            sscanf(line.c_str() + 3, "%f %f", &u, &v);
            attribs.texcoords.push_back(u);
            attribs.texcoords.push_back(v);
        }
        else if (trimmed.starts_with("vn "))
        {
            float nx, ny, nz;
            sscanf(line.c_str() + 3, "%f %f %f", &nx, &ny, &nz);
            attribs.normals.push_back(nx);
            attribs.normals.push_back(ny);
            attribs.normals.push_back(nz);
        }
        else if (trimmed.starts_with("l "))
        {
            // Line: l v1 v2 ... or l v1/vt1 v2/vt2 ...
            const char* ptr = line.c_str() + 2;
            std::vector<index_t> line_indices;
            line_indices.reserve(8);

            while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
            {
                int v = -1, vt = -1;
                while (*ptr == ' ' || *ptr == '\t') ++ptr;
                if (*ptr == '\0' || *ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                v = std::atoi(ptr);
                ptr += std::strcspn(ptr, "/ \t\r\n");

                if (*ptr == '/')
                {
                    ptr++;
                    vt = std::atoi(ptr);
                    ptr += std::strcspn(ptr, " \t\r\n");
                }

                index_t idx;
                idx.vertex_index = (v > 0) ? (v - 1) : UINT32_MAX;
                idx.texcoord_index = (vt > 0) ? (vt - 1) : UINT32_MAX;
                idx.normal_index = UINT32_MAX;

                line_indices.push_back(idx);
            }

            for (size_t i = 0; i + 1 < line_indices.size(); ++i)
            {
                indices.push_back(line_indices[i]);
                indices.push_back(line_indices[i + 1]);
            }
        }
        else if (trimmed.starts_with("f "))
        {
            // Face: f v/vt/vn v/vt/vn ... (also supports v, v/vt, v//vn)
            const char* ptr = line.c_str() + 2;
            std::vector<index_t> face_indices;
            face_indices.reserve(4);

            while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
            {
                int v = -1, vt = -1, vn = -1;
                while (*ptr == ' ' || *ptr == '\t') ++ptr;
                if (*ptr == '\0' || *ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                v = std::atoi(ptr);
                ptr += std::strcspn(ptr, "/ \t\r\n");

                if (*ptr == '/')
                {
                    ptr++;

                    if (*ptr == '/')
                    {
                        ptr++;
                        vn = std::atoi(ptr);
                        ptr += std::strcspn(ptr, " \t\r\n");
                    }
                    else
                    {
                        vt = std::atoi(ptr);
                        ptr += std::strcspn(ptr, "/ \t\r\n");

                        if (*ptr == '/')
                        {
                            ptr++;
                            vn = std::atoi(ptr);
                            ptr += std::strcspn(ptr, " \t\r\n");
                        }
                    }
                }

                index_t idx;
                idx.vertex_index = (v > 0) ? (v - 1) : UINT32_MAX;
                idx.texcoord_index = (vt > 0) ? (vt - 1) : UINT32_MAX;
                idx.normal_index = (vn > 0) ? (vn - 1) : UINT32_MAX;

                face_indices.push_back(idx);
            }

            // Triangulate
            if (face_indices.size() == 3)
            {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[1]);
                indices.push_back(face_indices[2]);
            }
            else if (face_indices.size() == 4)
            {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[1]);
                indices.push_back(face_indices[2]);

                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[2]);
                indices.push_back(face_indices[3]);
            }
            else if (face_indices.size() > 4)
            {
                for (size_t i = 1; i + 1 < face_indices.size(); ++i)
                {
                    indices.push_back(face_indices[0]);
                    indices.push_back(face_indices[i]);
                    indices.push_back(face_indices[i + 1]);
                }
            }
        }
    }

    auto end_timer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);

    std::cout << "Loaded 3D model " << filepath.filename() << " in " << duration.count() << "ms." << '\n';

    objFile.close();

    return {attribs, indices};
}

inline std::string ObjLoader::trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

} // Namespace gcep::objParser