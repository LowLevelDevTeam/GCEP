set(IMGUIZMO_SOURCES
        GraphEditor.cpp
        GraphEditor.h
        ImCurveEdit.cpp
        ImCurveEdit.h
        ImGradient.cpp
        ImGradient.h
        ImGuizmo.cpp
        ImGuizmo.h
        ImSequencer.cpp
        ImSequencer.h
        ImZoomSlider.h
)
add_library(ImGuizmo STATIC ${IMGUIZMO_SOURCES})
target_include_directories(ImGuizmo PUBLIC
        /
)
target_link_libraries(ImGuizmo PRIVATE ImGui)
message(STATUS "ImGuizmo: Version 1.83")