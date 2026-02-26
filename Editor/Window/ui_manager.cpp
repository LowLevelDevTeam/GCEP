#include "ui_manager.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <cmath>
#include <Editor/Camera/camera.hpp>

#include "imgui_internal.h"

namespace gcep {





void UiManager::initEditor()
{
    // Exemple pour C++
    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    // Texte de départ
    editor.SetText("// Écris ton code ici\nint main() {\n    return 0;\n}");
}

UiManager::UiManager(GLFWwindow* window, ImGui_ImplVulkan_InitInfo initInfo)
{
    m_window = window;

    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    IMGUI_CHECKVERSION();

    // Create context
    ImGui::CreateContext();

    // Get variables references
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // setup ImGui style
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;
    style.WindowPadding = ImVec2(0.0f, 0.0f);

    // Setup Platfrom/renderer backend
    ImGui_ImplGlfw_InitForVulkan(window, true);
    m_initInfo = initInfo;
    ImGui_ImplVulkan_Init(&m_initInfo);
}

void UiManager::setMeshList(std::vector<rhi::vulkan::VulkanMesh> &data)
{
    meshData  = &data;
}

void UiManager::setVieportResizeCallback(const std::function<void(uint32_t, uint32_t)>& callback)
{
    m_viewportResizeCallback = callback;
}

static bool DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
    bool value_changed = false;
    ImGuiIO& io = ImGui::GetIO();
    auto boldFont = io.Fonts->Fonts[0];

    ImGui::PushID(label.c_str());

    ImGui::Columns(2);
    ImGui::SetColumnWidth(0, columnWidth);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

    float lineHeight = GImGui->Font->LegacySize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

    // X
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("X", buttonSize)) {
        values.x = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Y
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Y", buttonSize)) {
        values.y = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Z
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
    ImGui::PushFont(boldFont);
    if (ImGui::Button("Z", buttonSize)) {
        values.z = resetValue;
        value_changed = true;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    if(ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) value_changed = true;
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();
    ImGui::Columns(1);
    ImGui::PopID();

    return value_changed;
}

void UiManager::uiUpdate(VkDescriptorSet sceneTexture, Camera* camera, uint32_t drawCount)
{
    if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
    {
        ImGui_ImplGlfw_Sleep(10);
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // Configurer ImGuizmo pour la frame courante
    ImGuizmo::BeginFrame();
    handleGizmoInput();


    // Enable docking over the entire viewport
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

    // Viewport window with the Vulkan rendered scene
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImVec2 availSize = ImGui::GetContentRegionAvail();

        // Check if viewport size changed (with a small threshold to avoid floating point issues)
        bool sizeChanged = std::abs(availSize.x - m_viewportSize.x) > 1.0f ||
                           std::abs(availSize.y - m_viewportSize.y) > 1.0f;

        if (sizeChanged && availSize.x > 0 && availSize.y > 0)
        {
            m_viewportSize = availSize;
            if (m_viewportResizeCallback)
            {
                m_viewportResizeCallback(
                    static_cast<uint32_t>(availSize.x),
                    static_cast<uint32_t>(availSize.y)
                );
                // Don't use the texture this frame since we just requested a resize
                // The texture will be valid next frame after processPendingOffscreenResize
                sceneTexture = VK_NULL_HANDLE;
            }
        }

        if (sceneTexture != VK_NULL_HANDLE && availSize.x > 0 && availSize.y > 0)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(sceneTexture), availSize);
        }
        else
        {
            // Display placeholder text when no texture is available
            ImGui::Text("Viewport loading...");
        }
        ImVec2 windowPos = ImGui::GetWindowPos();
        ImVec2 contentMin = ImGui::GetWindowContentRegionMin();
        ImGuizmo::SetRect(
            windowPos.x + contentMin.x,
            windowPos.y + contentMin.y,
            availSize.x,
            availSize.y
        );

        drawGizmo(camera);
    }
    ImGui::End();


    ImGui::Begin("Code Editor", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        editor.Render("TextEditor");

        // Bouton pour récupérer le texte et l’exécuter (exemple Lua/C++)
        if (ImGui::Button("Run Script"))
        {
            std::string code = editor.GetText();
            // Ici tu peux envoyer 'code' à Lua ou compiler en DLL
            printf("Script:\n%s\n", code.c_str());
        }

    }
    ImGui::End();


    if (showDemoWindow)
        ImGui::ShowDemoWindow(&showDemoWindow);

    {
        ImGui::Begin("Hello, world!");

        ImGui::Checkbox("Demo Window", &showDemoWindow);
        ImGui::ColorEdit4("ClearColor", (float*)&m_clearColor, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);

        ImGui::SeparatorText("Scene infos");
        ImGui::Text("Entities drawn : %d", drawCount);
        ImGui::DragFloat("Shininess", &shininess, 0.5f, 0.0f, 64.0f, "%.2f");

        ImGui::SeparatorText("Camera");
        ImGui::SliderFloat("Camera Speed", &camSpeed, 2.0f, 5.0f);
        ImGui::DragFloat3("Camera position", glm::value_ptr(camera->position));
        ImGui::DragFloat3("Camera front vector", glm::value_ptr(camera->front), 0.1f, -1.0f, 1.0f);
        ImGui::DragFloat("Camera yaw", &camera->yaw);
        ImGui::DragFloat("Camera pitch", &camera->pitch);

        ImGui::SeparatorText("Lights & normals");
        ImGui::ColorEdit3("Ambient color", glm::value_ptr(ambientColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        ImGui::ColorEdit3("Light color", glm::value_ptr(lightColor), ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueWheel);
        DrawVec3Control("Light direction", lightDirection);
        //ImGui::DragFloat3("Light direction", glm::value_ptr(lightDirection), 1.0f, -50.0f, 50.0f);

        auto& io = ImGui::GetIO();
        ImGui::SeparatorText("Application infos");
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
        ImGui::Text("Viewport size: %.0f x %.0f", m_viewportSize.x, m_viewportSize.y);

        ImGui::End();
    }

    {
        ImGui::Begin("Scene Hierarchy");

        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto& entity : *meshData) {
                bool is_selected = (m_SelectedEntityID == entity.id);
                if (ImGui::Selectable(entity.name.c_str(), is_selected)) {
                    m_SelectedEntityID = entity.id;
                }
            }
            ImGui::TreePop();
        }

        // Deselect if clicking in empty space
        if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
        {
            m_SelectedEntityID = std::numeric_limits<uint32_t>::max();
        }

        ImGui::End();

        ImGui::Begin("Entity properties");

        if (m_SelectedEntityID != std::numeric_limits<uint32_t>::max())
        {
            static float lodLevel = 0.0f;
            auto& data = *meshData;
            auto& entity = data[m_SelectedEntityID];
            DrawVec3Control("Translation", entity.transform.position);
            DrawVec3Control("Rotation", entity.transform.rotation);
            DrawVec3Control("Scale", entity.transform.scale);
            if(ImGui::DragFloat("LOD level", &lodLevel, 0.1f, 0.0f, static_cast<float>(entity.texture()->getMipLevels())))
            {
                entity.texture()->setLodLevel(lodLevel);
            }

            drawGizmoControls();
        }
        else
        {
            ImGui::Text("Select an entity to see its properties.");
        }
        ImGui::End();
    }
    perFrame.clearColor = m_clearColor;
    perFrame.ambientColor = ambientColor;
    perFrame.lightColor = lightColor;
    perFrame.lightDirection = lightDirection;
    perFrame.shininess = shininess;
}
void UiManager::drawGizmoControls()
{
    ImGui::Text("Gizmo");
    if (ImGui::RadioButton("Translate (W)", m_currentGizmoOperation == ImGuizmo::TRANSLATE)) {
        m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate (E)", m_currentGizmoOperation == ImGuizmo::ROTATE)) {
        m_currentGizmoOperation = ImGuizmo::ROTATE;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale (R)", m_currentGizmoOperation == ImGuizmo::SCALE)) {
        m_currentGizmoOperation = ImGuizmo::SCALE;
    }

    if (m_currentGizmoOperation == ImGuizmo::SCALE) {
        ImGui::TextDisabled("Mode: Local (forced for Scale)");
    } else {
        if (ImGui::RadioButton("Local", m_currentGizmoMode == ImGuizmo::LOCAL))
            m_currentGizmoMode = ImGuizmo::LOCAL;
        ImGui::SameLine();
        if (ImGui::RadioButton("World", m_currentGizmoMode == ImGuizmo::WORLD))
            m_currentGizmoMode = ImGuizmo::WORLD;
    }

    ImGui::Checkbox("Snap", &m_useSnap);

    // Afficher les contrôles de snapping selon l'opération courante
    if (m_useSnap) {
        switch (m_currentGizmoOperation) {
        case ImGuizmo::TRANSLATE:
            ImGui::DragFloat("Snapping X", &m_snapTranslation[0], 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Snapping Y", &m_snapTranslation[1], 0.1f, 0.0f, 100.0f);
            ImGui::DragFloat("Snapping Z", &m_snapTranslation[2], 0.1f, 0.0f, 100.0f);
            break;
        case ImGuizmo::ROTATE:
            ImGui::DragFloat("Snap Rotation", &m_snapRotation, 1.0f, 0.0f, 360.0f);
            break;
        case ImGuizmo::SCALE:
            ImGui::DragFloat("Snap Scale", &m_snapScale, 0.01f, 0.0f, 1.0f);
            break;
        default: break;
        }
    }
}

void UiManager::handleGizmoInput()
{
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow) || ImGuizmo::IsUsing())
        return;

    if (ImGui::IsKeyPressed(ImGuiKey_W))
        m_currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_E))
        m_currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        m_currentGizmoOperation = ImGuizmo::SCALE;
}

void UiManager::drawGizmo(Camera* camera)
{
    if (m_SelectedEntityID == std::numeric_limits<uint32_t>::max())
        return;
    if (!meshData || m_SelectedEntityID >= meshData->size())
        return;

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection = camera->getProjectionMatrix();
    projection[1][1] *= -1.0f;

    rhi::vulkan::VulkanMesh& selectedMesh = (*meshData)[m_SelectedEntityID];
    glm::mat4 modelMatrix = selectedMesh.getTransform();

    float modelData[16];
    memcpy(modelData, glm::value_ptr(modelMatrix), sizeof(float) * 16);

    float* snapPtr = nullptr;
    float snapValues[3] = {};
    if (m_useSnap) {
        switch (m_currentGizmoOperation) {
        case ImGuizmo::TRANSLATE:
            memcpy(snapValues, m_snapTranslation, sizeof(snapValues));
            break;
        case ImGuizmo::ROTATE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation;
            break;
        case ImGuizmo::SCALE:
            snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale;
            break;
        }
        snapPtr = snapValues;
    }

    glm::vec3 oldRotation = selectedMesh.transform.rotation;
    glm::vec3 oldScale = selectedMesh.transform.scale;

    float deltaData[16];
    glm::mat4 identityDelta = glm::mat4(1.0f);
    memcpy(deltaData, glm::value_ptr(identityDelta), sizeof(float) * 16);

    // Forcer LOCAL pour SCALE — le scale world n'est pas supporté par ImGuizmo
    ImGuizmo::MODE activeMode = (m_currentGizmoOperation == ImGuizmo::SCALE)
        ? ImGuizmo::LOCAL
        : m_currentGizmoMode;

    ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(projection),
        m_currentGizmoOperation,
        activeMode,
        modelData,
        deltaData,
        snapPtr
    );

    if (ImGuizmo::IsUsing()) {
        glm::mat4 newModel;
        memcpy(glm::value_ptr(newModel), modelData, sizeof(float) * 16);

        glm::mat4 delta;
        memcpy(glm::value_ptr(delta), deltaData, sizeof(float) * 16);

        switch (m_currentGizmoOperation) {
        case ImGuizmo::TRANSLATE: {
            selectedMesh.transform.position = glm::vec3(newModel[3]);
            break;
        }
        case ImGuizmo::ROTATE: {
            selectedMesh.transform.position = glm::vec3(newModel[3]);

            glm::vec3 scale;
            scale.x = glm::length(glm::vec3(newModel[0]));
            scale.y = glm::length(glm::vec3(newModel[1]));
            scale.z = glm::length(glm::vec3(newModel[2]));

            glm::mat3 r;
            r[0] = glm::vec3(newModel[0]) / scale.x;
            r[1] = glm::vec3(newModel[1]) / scale.y;
            r[2] = glm::vec3(newModel[2]) / scale.z;

            float sinX = glm::clamp(-r[2][1], -1.0f, 1.0f);
            selectedMesh.transform.rotation.x = asinf(sinX);
            selectedMesh.transform.rotation.y = atan2f(r[2][0], r[2][2]);
            selectedMesh.transform.rotation.z = atan2f(r[0][1], r[1][1]);
            break;
        }
        case ImGuizmo::SCALE: {
            // En mode LOCAL, le delta de scale est dans l'espace local de l'objet
            // On extrait directement le scale du delta
            glm::vec3 deltaScale;
            deltaScale.x = glm::length(glm::vec3(delta[0]));
            deltaScale.y = glm::length(glm::vec3(delta[1]));
            deltaScale.z = glm::length(glm::vec3(delta[2]));

            selectedMesh.transform.scale = oldScale * deltaScale;
            selectedMesh.transform.rotation = oldRotation;
            break;
        }
        default: break;
        }
    }
}
}
