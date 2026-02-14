#pragma once
#include "EntityComponent.hpp"
#include <vector>
#include <tuple>

namespace gcep
{
    class Registry;

    template <typename... Args>
    class View
    {
    private:
        Registry& m_registry;
        Signature m_viewSignature;
        IPool* m_smallestPool = nullptr;
        std::tuple<ComponentPool<Args>*...> m_pools;
        bool m_isExact;
    public:
        View(Registry& registry, bool exact);
        bool match(EntityID entity) const;


        struct Iterator {
            const EntityID* m_currentEntity;
            const EntityID* m_lastEntity;
            View& view;

            Iterator(const EntityID *current, const EntityID *last,  View &v);

            Iterator& operator++() ;
            EntityID operator*() const;
            bool operator!=( const Iterator& other)const;
        };

        Iterator begin();
        Iterator end();

        template<class T>
        T &get(EntityID entity);
    };
}

#include <Engine/Core/Entity-Component-System/detail/View.inl>
