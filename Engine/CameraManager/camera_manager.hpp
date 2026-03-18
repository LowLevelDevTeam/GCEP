

#pragma once

// Internals
#include <Editor/UI_Panels/editor_context.hpp>
#include <ECS/Components/camera_component.hpp>
#include <ECS/Components/transform.hpp>

namespace gcep::engine
{
    class CameraManager
    {
    public:
        void update(editor::EditorContext& ctx, float deltaTime);
    };
} // namespace gcep::engine