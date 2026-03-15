#include "project_loader.hpp"

// Externals
#include <imgui.h>
#include <Externals/rapidjson/document.h>
#include <Externals/rapidjson/istreamwrapper.h>
#include <Externals/rapidjson/prettywriter.h>
#include <Externals/rapidjson/stringbuffer.h>
#include <tinyfiledialogs.h>

// STL
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <iostream>

// Project
#include <Editor/Helpers.hpp>
#include <Editor/ContentBrowser/content_browser.hpp>

namespace gcep::pl
{

static std::filesystem::path getAppDataPath()
{
    const char* appData = std::getenv("APPDATA");
    std::filesystem::path base = appData
        ? std::filesystem::path(appData) / "GCEngine"
        : std::filesystem::current_path() / "GCEngine";
    if (!std::filesystem::exists(base))
        std::filesystem::create_directories(base);
    return base;
}

std::string ProjectLoader::nowISO8601()
{
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

void ProjectLoader::loadProject(const std::filesystem::path& projFile)
{
    std::ifstream ifs(projFile);
    if (!ifs.is_open()) return;

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document d;
    d.ParseStream(isw);
    if (d.HasParseError() || !d.IsObject()) return;

    auto getString = [&](const char* key, const char* def) -> std::string
    {
        return d.HasMember(key) && d[key].IsString() ? d[key].GetString() : def;
    };

    m_info.projectName = getString("name",       projFile.stem().string().c_str());
    m_info.version     = getString("version",    "1.0");
    m_info.createdAt   = getString("created_at", nowISO8601().c_str());

    if (d.HasMember("start_scene") && d["start_scene"].IsString())
        m_info.startScene = m_info.projectPath / d["start_scene"].GetString();

    if (!d.HasMember("settings") || !d["settings"].IsObject()) return;
    const rapidjson::Value& s = d["settings"];
    ProjectSettings& ps = m_info.settings;

    if (s.HasMember("vsync")           && s["vsync"].IsBool())        ps.vsync         = s["vsync"].GetBool();
    if (s.HasMember("camera_speed")    && s["camera_speed"].IsNumber()) ps.cameraSpeed  = s["camera_speed"].GetFloat();
    if (s.HasMember("taa_blend_alpha") && s["taa_blend_alpha"].IsNumber()) ps.taaBlendAlpha = s["taa_blend_alpha"].GetFloat();

    auto loadFloat3 = [&](const char* key, float* dst)
    {
        if (s.HasMember(key) && s[key].IsArray() && s[key].Size() == 3)
            for (int i = 0; i < 3; i++) dst[i] = s[key][i].GetFloat();
    };
    auto loadFloat4 = [&](const char* key, float* dst)
    {
        if (s.HasMember(key) && s[key].IsArray() && s[key].Size() == 4)
            for (int i = 0; i < 4; i++) dst[i] = s[key][i].GetFloat();
    };

    loadFloat4("clear_color",     ps.clearColor);
    loadFloat3("ambient_color",   ps.ambientColor);
    loadFloat3("light_color",     ps.lightColor);
    loadFloat3("light_direction", ps.lightDirection);

    if (s.HasMember("grid") && s["grid"].IsObject())
    {
        const rapidjson::Value& g = s["grid"];
        if (g.HasMember("cell_size")     && g["cell_size"].IsNumber())     ps.gridCellSize     = g["cell_size"].GetFloat();
        if (g.HasMember("thick_every")   && g["thick_every"].IsNumber())   ps.gridThickEvery   = g["thick_every"].GetFloat();
        if (g.HasMember("fade_distance") && g["fade_distance"].IsNumber()) ps.gridFadeDistance = g["fade_distance"].GetFloat();
        if (g.HasMember("line_width")    && g["line_width"].IsNumber())    ps.gridLineWidth    = g["line_width"].GetFloat();
    }

    m_info.contentPath = m_info.projectPath / "Content";
    std::filesystem::create_directories(m_info.contentPath);
}

void ProjectLoader::writeProjectFile()
{
    if (m_info.projectPath.empty() || m_info.projectName.empty()) return;

    ProjectSettings& ps = m_info.settings;

    rapidjson::Document d;
    d.SetObject();
    rapidjson::Document::AllocatorType& a = d.GetAllocator();

    auto addString = [&](rapidjson::Value& obj, const char* key, const std::string& val)
    {
        obj.AddMember(
            rapidjson::Value(key, a),
            rapidjson::Value(val.c_str(), a),
            a);
    };
    auto addFloat3 = [&](rapidjson::Value& obj, const char* key, float* v)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (int i = 0; i < 3; i++) arr.PushBack(v[i], a);
        obj.AddMember(rapidjson::Value(key, a), arr, a);
    };
    auto addFloat4 = [&](rapidjson::Value& obj, const char* key, float* v)
    {
        rapidjson::Value arr(rapidjson::kArrayType);
        for (int i = 0; i < 4; i++) arr.PushBack(v[i], a);
        obj.AddMember(rapidjson::Value(key, a), arr, a);
    };

    addString(d, "name",        m_info.projectName);
    addString(d, "version",     m_info.version);
    addString(d, "root",        m_info.projectPath.string());
    addString(d, "created_at",  m_info.createdAt.empty() ? nowISO8601() : m_info.createdAt);
    addString(d, "last_saved",  nowISO8601());
    addString(d, "start_scene", std::filesystem::relative(m_info.startScene, m_info.projectPath).string());

    rapidjson::Value settings(rapidjson::kObjectType);
    settings.AddMember("vsync",           ps.vsync,         a);
    settings.AddMember("camera_speed",    ps.cameraSpeed,   a);
    settings.AddMember("taa_blend_alpha", ps.taaBlendAlpha, a);
    addFloat4(settings, "clear_color",     ps.clearColor);
    addFloat3(settings, "ambient_color",   ps.ambientColor);
    addFloat3(settings, "light_color",     ps.lightColor);
    addFloat3(settings, "light_direction", ps.lightDirection);

    rapidjson::Value grid(rapidjson::kObjectType);
    grid.AddMember("cell_size",     ps.gridCellSize,     a);
    grid.AddMember("thick_every",   ps.gridThickEvery,   a);
    grid.AddMember("fade_distance", ps.gridFadeDistance, a);
    grid.AddMember("line_width",    ps.gridLineWidth,    a);
    settings.AddMember("grid", grid, a);

    d.AddMember("settings", settings, a);

    rapidjson::StringBuffer sb;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sb);
    d.Accept(writer);

    std::ofstream ofs(m_info.projectPath / (m_info.projectName + ".gcproj"));
    ofs << sb.GetString();

    m_dirty      = false;
    m_dirtyTimer = 0.0;
}

void ProjectLoader::saveProject() { writeProjectFile(); }

ProjectLoader& ProjectLoader::instance()
{
    static ProjectLoader inst;
    return inst;
}

void ProjectLoader::init(GLFWwindow* window)
{
    m_window = window;
}

void ProjectLoader::update(double deltaTime)
{
    if (m_info.projectPath.empty()) return;

    if (m_dirty)
    {
        m_dirtyTimer += deltaTime;
        if (m_dirtyTimer >= m_dirtyDebounce)
            saveProject();
    }

    m_autoSaveTimer += deltaTime;
    if (m_autoSaveTimer >= m_autoSaveInterval)
    {
        saveProject();
        m_autoSaveTimer = 0.0;
    }
}

void ProjectLoader::drawUI(bool& stillSelecting)
{
    ImGui::Begin("Project Loader", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("Select a project to open:");

    if (ImGui::Button("Open project"))
    {
        const char* filters[] = { "*.gcproj" };
        if (auto path = tinyfd_openFileDialog("Open a project",
            std::filesystem::current_path().string().c_str(), 1, filters, "GC Project", 0);
            path != nullptr)
        {
            std::filesystem::path p(path);
            m_info.projectPath = p.parent_path();
            m_info.contentPath = m_info.projectPath / "Content";
            std::filesystem::create_directories(m_info.contentPath);
            stillSelecting = false;
            loadProject(p);
        }
    }

    ImGui::InputText("Project name", m_projectName, sizeof(m_projectName));

    if (ImGui::Button("Create project"))
    {
        if (m_projectName[0] == '\0')
        {
            ImGui::OpenPopup("Error");
        }
        else
        {
            std::filesystem::path projectPath = getAppDataPath() / m_projectName;
            std::filesystem::create_directories(projectPath / "Content");
            std::filesystem::create_directories(projectPath / "Content" / "Scenes");
            m_info.projectPath = projectPath;
            m_info.projectName = m_projectName;
            m_info.contentPath = projectPath / "Content";
            m_info.startScene  = projectPath / "Content" / "Scenes" / "default.gcmap";
            m_info.createdAt   = nowISO8601();
            writeProjectFile();
            stillSelecting = false;
        }
    }

    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Please enter a project name.");
        if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::End();
}

} // namespace gcep::pl
