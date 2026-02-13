#include "ObjParser.hpp"

#include <algorithm>
#include <cassert>
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

    FILE* objFile = std::fopen(filepath.string().c_str(), "r");

    if (!objFile)
    {
        throw std::runtime_error("Couldn't load 3D model!");
    }
    auto start_timer = std::chrono::high_resolution_clock::now();

    char line[4096];

    // First pass: count elements
    unsigned int vCount = 0, vtCount = 0, vnCount = 0, fCount = 0, lCount = 0;
    while (std::fgets(line, sizeof(line), objFile))
    {
        char* ptr = line;

        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        if (*ptr == '\0' || *ptr == '\n' || *ptr == '#') continue;

        if (ptr[0] == 'v' && ptr[1] == ' ')       ++vCount;
        else if (ptr[0] == 'v' && ptr[1] == 't')  ++vtCount;
        else if (ptr[0] == 'v' && ptr[1] == 'n')  ++vnCount;
        else if (ptr[0] == 'f' && ptr[1] == ' ')  ++fCount;
        else if (ptr[0] == 'l' && ptr[1] == ' ')  ++lCount;
    }
    std::rewind(objFile);

    attrib_t attribs;
    std::vector<index_t> indices;

    attribs.vertices.reserve(vCount * 3);
    attribs.texcoords.reserve(vtCount * 2);
    attribs.normals.reserve(vnCount * 3);
    indices.reserve(fCount * 6 + lCount * 8);

    std::cout << "Model " << filepath.filename() << " :" << '\n'
    << " - Vertex count: " << vCount << '\n'
    << " - Texture count: " << vtCount << '\n'
    << " - Normal count: " << vnCount << '\n'
    << " - Face count: " << fCount << '\n';

    // Second pass: parse data
    while (std::fgets(line, sizeof(line), objFile))
    {
        char* ptr = line;

        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        if (*ptr == '\0' || *ptr == '\n' || *ptr == '#') continue;

        if (ptr[0] == 'v' && ptr[1] == ' ')
        {
            float x, y, z;
            sscanf(ptr + 2, "%f %f %f", &x, &y, &z);
            attribs.vertices.push_back(x);
            attribs.vertices.push_back(y);
            attribs.vertices.push_back(z);
        }
        else if (ptr[0] == 'v' && ptr[1] == 't')
        {
            float u, v;
            sscanf(ptr + 3, "%f %f", &u, &v);
            attribs.texcoords.push_back(u);
            attribs.texcoords.push_back(v);
        }
        else if (ptr[0] == 'v' && ptr[1] == 'n')
        {
            float nx, ny, nz;
            sscanf(ptr + 3, "%f %f %f", &nx, &ny, &nz);
            attribs.normals.push_back(nx);
            attribs.normals.push_back(ny);
            attribs.normals.push_back(nz);
        }
        else if (ptr[0] == 'l' && ptr[1] == ' ')
        {
            // Line: l v1 v2 ... or l v1/vt1 v2/vt2 ...
            ptr += 2;

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

                indices.push_back(idx);
            }
        }
        else if (ptr[0] == 'f' && ptr[1] == ' ')
        {
            // Face: f v/vt/vn v/vt/vn ... (also supports v, v/vt, v//vn)
            ptr += 2;
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
            // Quads
            else if (face_indices.size() == 4)
            {
                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[1]);
                indices.push_back(face_indices[2]);

                indices.push_back(face_indices[0]);
                indices.push_back(face_indices[2]);
                indices.push_back(face_indices[3]);
            }
            // NGon (Triangle Strip)
            else if (face_indices.size() > 4)
            {
                for (size_t i = 0; i + 2 < face_indices.size(); ++i)
                {
                    indices.push_back(face_indices[i]);
                    indices.push_back(face_indices[i + 1]);
                    indices.push_back(face_indices[i + 2]);
                }
            }
        }
    }

    auto end_timer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);

    std::cout << "Loaded 3D model " << filepath.filename() << " in " << duration.count() << "ms." << '\n';

    std::fclose(objFile);

    return {attribs, indices};
}

inline std::string ObjLoader::trim(const std::string& s)
{
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

} // Namespace gcep::objParser