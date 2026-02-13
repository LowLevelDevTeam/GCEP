#include "ObjParser.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>

namespace gcep::objParser
{

std::pair<attrib_t, std::vector<index_t>> loadObj(std::filesystem::path& filepath)
{
    assert(!filepath.string().empty() && "Invalid file path!");
    assert(filepath.extension() == ".obj" && "File must be .obj!");

    auto start_timer = std::chrono::high_resolution_clock::now();

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file || !file.is_open())
    {
        throw std::runtime_error("Couldn't load 3D model!");
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size + 1);
    if (!file.read(buffer.data(), size))
    {
        throw std::runtime_error("Failed to read file!");
    }
    buffer[size] = '\0';

    attrib_t attribs;
    std::vector<index_t> indices;

    // Reserve with reasonable estimates
    attribs.vertices.reserve(100000);
    attribs.texcoords.reserve(100000);
    attribs.normals.reserve(100000);
    indices.reserve(300000);

    char* ptr = buffer.data();
    char* end = ptr + size;

    auto parseFloat = [](char*& p) -> float
    {
        float result = 0.0f;
        float sign = 1.0f;

        if (*p == '-')
        {
            sign = -1.0f; ++p;
        }
        else if (*p == '+') ++p;

        while (*p >= '0' && *p <= '9')
        {
            result = result * 10.0f + (*p - '0');
            ++p;
        }

        if (*p == '.')
        {
            ++p;
            float frac = 0.1f;
            while (*p >= '0' && *p <= '9')
            {
                result += (*p - '0') * frac;
                frac *= 0.1f;
                ++p;
            }
        }

        // Handle scientific notation (e.g., 1.5e-3)
        if (*p == 'e' || *p == 'E') {
            ++p;
            int exp_sign = 1;
            if (*p == '-')
            {
                exp_sign = -1; ++p;
            }
            else if (*p == '+') ++p;

            int exponent = 0;
            while (*p >= '0' && *p <= '9')
            {
                exponent = exponent * 10 + (*p - '0');
                ++p;
            }

            result *= std::pow(10.0f, exp_sign * exponent);
        }

        return result * sign;
    };

    auto parseInt = [](char*& p) -> int
    {
        int result = 0;
        bool negative = false;

        if (*p == '-')
        {
            negative = true; ++p;
        }

        while (*p >= '0' && *p <= '9')
        {
            result = result * 10 + (*p - '0');
            ++p;
        }

        return negative ? -result : result;
    };

    auto skipWhitespace = [](char*& p)
    {
        while (*p == ' ' || *p == '\t')
        {
            ++p;
        }
    };

    auto skipLine = [](char*& p, char* end)
    {
        while (p < end && *p != '\n')
        {
            ++p;
        }
        if (p < end)
        {
            ++p;
        }
    };

    unsigned int vCount = 0, vtCount = 0, vnCount = 0, fCount = 0;

    while (ptr < end)
    {
        skipWhitespace(ptr);

        if (*ptr == '\0' || *ptr == '\n' || *ptr == '#')
        {
            skipLine(ptr, end);
            continue;
        }

        if (ptr[0] == 'v')
        {
            if (ptr[1] == ' ')
            {
                // Vertex: v x y z
                ptr += 2;
                skipWhitespace(ptr);
                float x = parseFloat(ptr);
                skipWhitespace(ptr);
                float y = parseFloat(ptr);
                skipWhitespace(ptr);
                float z = parseFloat(ptr);

                attribs.vertices.push_back(x);
                attribs.vertices.push_back(y);
                attribs.vertices.push_back(z);
                ++vCount;
            }
            else if (ptr[1] == 't')
            {
                // UV: vt x y
                ptr += 2;
                skipWhitespace(ptr);
                float u = parseFloat(ptr);
                skipWhitespace(ptr);
                float v = parseFloat(ptr);

                attribs.texcoords.push_back(u);
                attribs.texcoords.push_back(v);
                ++vtCount;
            }
            else if (ptr[1] == 'n')
            {
                // Normal: vn x y z
                ptr += 3;
                skipWhitespace(ptr);
                float nx = parseFloat(ptr);
                skipWhitespace(ptr);
                float ny = parseFloat(ptr);
                skipWhitespace(ptr);
                float nz = parseFloat(ptr);

                attribs.normals.push_back(nx);
                attribs.normals.push_back(ny);
                attribs.normals.push_back(nz);
                ++vnCount;
            }
        }
        else if (ptr[0] == 'l' && ptr[1] == ' ')
        {
            // Line: l v1 v2 ... or l v1/vt1 v2/vt2 ...
            ptr += 2;
            while (ptr < end && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
            {
                skipWhitespace(ptr);
                if (*ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                int v = parseInt(ptr);
                int vt = -1;

                if (*ptr == '/')
                {
                    ++ptr;
                    vt = parseInt(ptr);
                }

                indices.push_back({
                    .vertex_index   = (v > 0)  ? (v - 1)  : UINT32_MAX,
                    .texcoord_index = (vt > 0) ? (vt - 1) : UINT32_MAX,
                    .normal_index   = UINT32_MAX
                });
            }
        }
        else if (ptr[0] == 'f' && ptr[1] == ' ')
        {
            // Face: f v/vt/vn ... (also supports v, v/vt, v//vn)
            ptr += 2;
            std::vector<index_t> face_indices;
            face_indices.reserve(4);

            while (ptr < end && *ptr != '\n' && *ptr != '\r' && *ptr != '#')
            {
                skipWhitespace(ptr);
                if (*ptr == '\n' || *ptr == '\r' || *ptr == '#') break;

                int v = parseInt(ptr);
                int vt = -1, vn = -1;

                if (*ptr == '/')
                {
                    ++ptr;
                    if (*ptr == '/')
                    {
                        ++ptr;
                        vn = parseInt(ptr);
                    } else
                    {
                        vt = parseInt(ptr);
                        if (*ptr == '/')
                        {
                            ++ptr;
                            vn = parseInt(ptr);
                        }
                    }
                }

                face_indices.push_back({
                   .vertex_index   = (v > 0)  ? (v - 1)  : UINT32_MAX,
                   .texcoord_index = (vt > 0) ? (vt - 1) : UINT32_MAX,
                   .normal_index   = (vn > 0) ? (vn - 1) : UINT32_MAX
                });
            }

            // Triangulate
            size_t face_count = face_indices.size();
            if (face_count >= 3)
            {
                for (size_t i = 1; i + 1 < face_count; ++i)
                {
                    indices.push_back(face_indices[0]);
                    indices.push_back(face_indices[i]);
                    indices.push_back(face_indices[i + 1]);
                }
                ++fCount;
            }
        }

        skipLine(ptr, end);
    }

    auto end_timer = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_timer - start_timer);

    std::cout << "Model " << filepath.filename() << " :" << '\n'
              << " - Size: " << (buffer.size() * sizeof(char)) << " bytes" << '\n'
              << " - Vertices: " << vCount  << '\n'
              << " - UVs: "      << vtCount << '\n'
              << " - Normals: "  << vnCount << '\n'
              << " - Faces: "    << fCount  << '\n';

    std::cout << "Loaded 3D model " << filepath.filename() << " in " << duration.count() << "ms." << '\n';

    return {attribs, indices};
}

} // Namespace gcep::objParser