#pragma once

#include <Editor/UI_Panels/ipanel.hpp>
#include <Editor/UI_Panels/editor_context.hpp>
#include <ECS/headers/entity_component.hpp>

#include <string>

namespace gcep::panel
{
    class SceneHierarchyPanel : public IPanel
    {
    public:
        void draw() override;

    private:
        void drawSpawnMenu();
        void drawEntityNode(ECS::EntityID id);
        void drawEntityContextMenu(ECS::EntityID id);
        void removeSelected();

        /// @brief Returns "icon name" for an entity, searching meshes and lights.
        std::string getEntityLabel(ECS::EntityID id) const;
    };

} // namespace gcep::panel
