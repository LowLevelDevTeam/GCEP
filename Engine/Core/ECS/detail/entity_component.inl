#pragma once
#include <Engine/Core/ECS/headers/entity_component.hpp>



namespace gcep::ECS
{
    inline EntityID EntityIDGenerator::generateID()
    {
        if (nextAvailable == INVALID_VALUE)
        {
            grow(elements.empty() ? 10000 : elements.size() * 2);
        }
        std::uint32_t index = nextAvailable;

        nextAvailable = elements[index].nextfreeID;
        elements[index].active = true;

        EntityID id = (static_cast<EntityID>(elements[index].version) << 24) | (index & 0xFFFFFF);
        return id;
    }

    inline void EntityIDGenerator::destroyEntity(EntityID id)
    {
        std::uint32_t index = id & 0xFFFFFF;
        if (index >= elements.size() || !elements[index].active) return;
        elements[index].active = false;
        elements[index].version++;
        if (elements[index].version == 0)
        {
            elements[index].version = 1;
        }
        elements[index].nextfreeID = nextAvailable;
        nextAvailable = index;
    }

    inline bool EntityIDGenerator::isValid(EntityID id)
    {
        std::uint32_t index = id & 0xFFFFFF;
        auto versionInID = static_cast<std::uint8_t>(id >> 24);
        if (index >= elements.size()) return false;
        return elements[index].active && (elements[index].version == versionInID);
    }

    inline void EntityIDGenerator::grow(std::size_t newSize)
    {
        size_t oldSize = elements.size();
        elements.resize(newSize);
        for (size_t i = oldSize; i < newSize - 1; i++)
        {
            elements[i].nextfreeID = static_cast<std::uint32_t>(i+1);
            elements[i].active = false;
        }
        elements[newSize-1].active = false;
        elements[newSize-1].nextfreeID = nextAvailable;

        nextAvailable = static_cast<std::uint32_t>(oldSize);
    }

    inline EntityID EntityIDGenerator::forceID(EntityID id)
    {
        std::uint32_t index = id & 0xFFFFFF;
        std::uint8_t version = static_cast<std::uint8_t>(id >> 24);

        if (index >= elements.size())
            grow(index + 1);

        auto& element = elements[index];
        if (element.active)
            return id;

        element.active = true;
        element.version = version;

        if (nextAvailable == index) {
            nextAvailable = element.nextfreeID;
        } else {
            EntityID curr = nextAvailable;
            while (curr != INVALID_VALUE && elements[curr].nextfreeID != index)
                curr = elements[curr].nextfreeID;
            if (curr != INVALID_VALUE)
                elements[curr].nextfreeID = element.nextfreeID;
        }

        return id;
    }

}

