#pragma once
#include "ECS/Components/Transform.hpp"

namespace gcep::UI
{
    class SelectionManager
    {
    public:
        void select(ECS::EntityID id)
        {
            m_selectedEntityID = id;
        }
        void unselect()
        {
            m_selectedEntityID = ECS::INVALID_VALUE;
        }

        ECS::EntityID getSelectedEntityID() const {return m_selectedEntityID;}

        bool hasSelectedEntity() const {return m_selectedEntityID != ECS::INVALID_VALUE;}

    private:
        ECS::EntityID m_selectedEntityID = ECS::INVALID_VALUE;
    };
}
