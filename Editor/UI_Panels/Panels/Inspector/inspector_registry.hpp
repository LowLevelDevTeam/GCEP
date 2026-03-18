#pragma once

/// @file inspector_registry.hpp
/// @brief Editor-side registry mapping component types to their ImGui draw functions.
///
/// Keeps the engine completely independent of ImGui.
/// Register a drawer once at startup; InspectorPanel calls drawAll() each frame.

#include <ECS/headers/registry.hpp>
#include <ECS/headers/entity_component.hpp>
#include <ECS/headers/component_registry.hpp>
#include <Editor/UI_Panels/component_drawers.hpp>

#include <boost/pfr/core.hpp>
#include <boost/pfr/core_name.hpp>
#include <imgui.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

namespace gcep::editor
{
    /// @class InspectorRegistry
    /// @brief Singleton that maps each component typeID to a draw function.
    ///
    /// Components are drawn in registration order (insertion order preserved).
    /// Default registration uses Boost.PFR to iterate fields automatically.
    /// A custom lambda can override the default for components that need
    /// specific widgets (combos, sliders with range, etc.).
    ///
    /// Each component is rendered inside a CollapsingHeader, open by default.
    class InspectorRegistry
    {
    public:
        static InspectorRegistry& get()
        {
            static InspectorRegistry instance;
            return instance;
        }

        /// @brief Registers a default PFR-based drawer for component type T.
        /// @param removable  Whether the component can be removed from the inspector.
        template<typename T>
        void registerDrawer(bool removable = true)
        {
            const uint32_t id   = ECS::ComponentIDGenerator::get<T>();
            const auto&    name = getComponentName<T>();

            registerEntry({
                id,
                name,
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    auto& component = registry.getComponent<T>(entity);
                    boost::pfr::for_each_field(component, [](auto& field, auto idx)
                    {
                        constexpr auto fieldName = boost::pfr::get_name<idx, T>();
                        drawField(field, fieldName.data());
                    });
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    if (!registry.hasComponent<T>(entity))
                        registry.addComponent<T>(entity);
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    return registry.hasComponent<T>(entity);
                },
                removable
                    ? std::function<void(ECS::Registry&, ECS::EntityID)>(
                        [](ECS::Registry& registry, ECS::EntityID entity)
                        {
                            registry.removeComponent<T>(entity);
                        })
                    : nullptr
            });
        }

        /// @brief Registers a custom drawer for component type T.
        /// @param customDraw  Callable invoked each frame to render the component's fields.
        /// @param removable   Whether the component can be removed from the inspector.
        template<typename T, typename Fn>
            requires std::invocable<Fn, T&>
        void registerDrawer(Fn&& customDraw, bool removable = true)
        {
            const uint32_t id   = ECS::ComponentIDGenerator::get<T>();
            const auto&    name = getComponentName<T>();

            std::function<void(T&)> fn = std::forward<Fn>(customDraw);
            registerEntry({
                id,
                name,
                [fn](ECS::Registry& registry, ECS::EntityID entity)
                {
                    auto& component = registry.getComponent<T>(entity);
                    fn(component);
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    if (!registry.hasComponent<T>(entity))
                        registry.addComponent<T>(entity);
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    return registry.hasComponent<T>(entity);
                },
                removable
                    ? std::function<void(ECS::Registry&, ECS::EntityID)>(
                        [](ECS::Registry& registry, ECS::EntityID entity)
                        {
                            registry.removeComponent<T>(entity);
                        })
                    : nullptr
            });
        }

        /// @brief Draws all registered components present on the given entity, in registration order.
        void drawAll(ECS::Registry& registry, ECS::EntityID entity)
        {
            for (auto& entry : m_entries)
                drawEntry(entry, registry, entity);
        }

        /// @brief Draws only the component of type T if present on the entity.
        template<typename T>
        void drawComponent(ECS::Registry& registry, ECS::EntityID entity)
        {
            const uint32_t id = ECS::ComponentIDGenerator::get<T>();
            auto it = std::find_if(m_entries.begin(), m_entries.end(),
                [id](const Entry& e) { return e.typeID == id; });
            if (it != m_entries.end())
                drawEntry(*it, registry, entity);
        }

        /// @brief Draws all registered components except T, in registration order.
        template<typename T>
        void drawAllExcept(ECS::Registry& registry, ECS::EntityID entity)
        {
            const uint32_t excludeId = ECS::ComponentIDGenerator::get<T>();
            for (auto& entry : m_entries)
            {
                if (entry.typeID == excludeId) continue;
                drawEntry(entry, registry, entity);
            }
        }

        /// @brief Renders an "Add component" button + popup listing missing components.
        void drawComponentAdder(ECS::Registry& registry, ECS::EntityID entity)
        {
            if (ImGui::Button("+ Add component"))
                ImGui::OpenPopup("##AddComponentPopup");

            if (ImGui::BeginPopup("##AddComponentPopup"))
            {
                int shown = 0;
                for (auto& entry : m_entries)
                {
                    if (entry.has(registry, entity)) continue;
                    shown++;
                    if (ImGui::MenuItem(entry.name.c_str()))
                        entry.add(registry, entity);
                }
                if (shown == 0)
                    ImGui::TextDisabled("All components already added");
                ImGui::EndPopup();
            }
        }

        /// @brief Renders a "Remove component" button + popup listing removable components present on the entity.
        void drawComponentRemover(ECS::Registry& registry, ECS::EntityID entity)
        {
            if (ImGui::Button("- Remove component"))
                ImGui::OpenPopup("##RemoveComponentPopup");

            if (ImGui::BeginPopup("##RemoveComponentPopup"))
            {
                int shown = 0;
                for (auto& entry : m_entries)
                {
                    if (!entry.remove) continue;
                    if (!entry.has(registry, entity)) continue;
                    shown++;
                    if (ImGui::MenuItem(entry.name.c_str()))
                        entry.remove(registry, entity);
                }
                if (shown == 0)
                    ImGui::TextDisabled("No removable components");
                ImGui::EndPopup();
            }
        }

    private:
        InspectorRegistry() = default;

        struct Entry
        {
            uint32_t                                                 typeID;
            std::string                                              name;
            std::function<void(ECS::Registry&, ECS::EntityID)>      draw;   ///< Renders fields only (no header).
            std::function<void(ECS::Registry&, ECS::EntityID)>      add;
            std::function<bool(ECS::Registry&, ECS::EntityID)>      has;
            std::function<void(ECS::Registry&, ECS::EntityID)>      remove; ///< nullptr if not removable.
        };

        /// @brief Renders one entry as a CollapsingHeader wrapping its fields.
        void drawEntry(const Entry& entry, ECS::Registry& registry, ECS::EntityID entity)
        {
            if (!entry.has(registry, entity)) return;

            ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(0.22f, 0.22f, 0.22f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(0.38f, 0.38f, 0.38f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.f,   1.f,   1.f,   1.f));
            const bool open = ImGui::CollapsingHeader(entry.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
            ImGui::PopStyleColor(4);

            if (open)
                entry.draw(registry, entity);
        }

        /// @brief Inserts a new entry or replaces the existing one with the same typeID.
        void registerEntry(Entry entry)
        {
            auto it = std::find_if(m_entries.begin(), m_entries.end(),
                [&entry](const Entry& e) { return e.typeID == entry.typeID; });
            if (it != m_entries.end())
                *it = std::move(entry);
            else
                m_entries.push_back(std::move(entry));
        }

        template<typename T>
        static const std::string& getComponentName()
        {
            for (const auto& entry : ECS::ComponentRegistry::instance().entries())
            {
                if (entry.typeID == ECS::ComponentIDGenerator::get<T>())
                {
                    static std::string shortName;
                    const auto pos = entry.name.rfind("::");
                    shortName = (pos != std::string::npos) ? entry.name.substr(pos + 2) : entry.name;
                    return shortName;
                }
            }
            static std::string fallback = typeid(T).name();
            return fallback;
        }

        std::vector<Entry> m_entries;
    };

} // namespace gcep::editor
