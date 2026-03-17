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

#include <functional>
#include <iostream>
#include <unordered_map>

namespace gcep::editor
{
    /// @class InspectorRegistry
    /// @brief Singleton that maps each component typeID to a draw function.
    ///
    /// Default registration uses Boost.PFR to iterate fields automatically.
    /// A custom lambda can override the default for components that need
    /// specific widgets (combos, sliders with range, etc.).
    class InspectorRegistry
    {
    public:
        static InspectorRegistry& get()
        {
            static InspectorRegistry instance;
            return instance;
        }

        /// @brief Registers a default PFR-based drawer for component type T.
        template<typename T>
        void registerDrawer()
        {
            const uint32_t id   = ECS::ComponentIDGenerator::get<T>();
            const auto&    name = getComponentName<T>();

            m_entries[id] = {
                name,
                [name](ECS::Registry& registry, ECS::EntityID entity)
                {
                    if (!registry.hasComponent<T>(entity)) return;
                    auto& component = registry.getComponent<T>(entity);
                    ImGui::SeparatorText(name.c_str());
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
                }
            };
        }

        /// @brief Registers a custom drawer for component type T.
        template<typename T>
        void registerDrawer(std::function<void(T&)> customDraw)
        {
            const uint32_t id   = ECS::ComponentIDGenerator::get<T>();
            const auto&    name = getComponentName<T>();

            m_entries[id] = {
                name,
                [name, customDraw](ECS::Registry& registry, ECS::EntityID entity)
                {
                    if (!registry.hasComponent<T>(entity)) return;
                    auto& component = registry.getComponent<T>(entity);
                    ImGui::SeparatorText(name.c_str());
                    customDraw(component);
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    if (!registry.hasComponent<T>(entity))
                        registry.addComponent<T>(entity);
                },
                [](ECS::Registry& registry, ECS::EntityID entity)
                {
                    return registry.hasComponent<T>(entity);
                }
            };
        }

        /// @brief Draws all registered components present on the given entity.
        void drawAll(ECS::Registry& registry, ECS::EntityID entity)
        {
            for (auto& [typeID, entry] : m_entries)
                entry.draw(registry, entity);
        }

        /// @brief Renders an "Add component" button + popup listing missing components.
        void drawComponentAdder(ECS::Registry& registry, ECS::EntityID entity)
        {
            if (ImGui::Button("+ Add component"))
            {
                std::cout << "[AddComponent] clicked, entries=" << m_entries.size() << "\n";
                for (auto& [typeID, entry] : m_entries)
                    std::cout << "  - " << entry.name << " has=" << entry.has(registry, entity) << "\n";
                ImGui::OpenPopup("##AddComponentPopup");
            }

            if (ImGui::BeginPopup("##AddComponentPopup"))
            {
                int shown = 0;
                for (auto& [typeID, entry] : m_entries)
                {
                    std::cout << "[AddComponent] entry=" << entry.name << " has=" << entry.has(registry, entity) << "\n";
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

    private:
        InspectorRegistry() = default;

        struct Entry
        {
            std::string                                              name;
            std::function<void(ECS::Registry&, ECS::EntityID)>      draw;
            std::function<void(ECS::Registry&, ECS::EntityID)>      add;
            std::function<bool(ECS::Registry&, ECS::EntityID)>      has;
        };

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

        std::unordered_map<uint32_t, Entry> m_entries;
    };

} // namespace gcep::editor
