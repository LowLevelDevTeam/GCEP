#include "transform_component.hpp"

#include "Engine/Core/Entity-Component-System/headers/registry.hpp"

namespace gcep
{
    void TransformComponentManager::updateAllComponents()
    {
        // Temp registry (Registry::getInstance()) dans le futur
        Registry reg;

        auto view = reg.partialView<TransformComponent>();

        for (EntityID id : view)
        {
            auto& transform = view.get<TransformComponent>(id);
            transform.position += {1.f, 1.f, 1.f}; // Modification de la position a chaque frame
        }
    }
} // gcep